/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2005 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Zeev Suraski <zeev@zend.com>                                |
   |          Tom May <tom@go2net.com>                                    |
   |          Timm Friebe <php_sybase_ct@thekid.de>                       |
   +----------------------------------------------------------------------+
 */

/* $Id$ */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_sybase_ct.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/info.h"
#include "php_globals.h"
#include "php_ini.h"

/* True globals, no need for thread safety */
static int le_link, le_plink, le_result;

#if HAVE_SYBASE_CT

zend_function_entry sybase_functions[] = {
	PHP_FE(sybase_connect, NULL)
	PHP_FE(sybase_pconnect, NULL)
	PHP_FE(sybase_close, NULL)
	PHP_FE(sybase_select_db, NULL)
	PHP_FE(sybase_query, NULL)
	PHP_FE(sybase_unbuffered_query, NULL)
	PHP_FE(sybase_free_result, NULL)
	PHP_FE(sybase_get_last_message, NULL)
	PHP_FE(sybase_num_rows, NULL)
	PHP_FE(sybase_num_fields, NULL)
	PHP_FE(sybase_fetch_row, NULL)
	PHP_FE(sybase_fetch_array, NULL)
	PHP_FE(sybase_fetch_assoc, NULL)
	PHP_FE(sybase_fetch_object, NULL)
	PHP_FE(sybase_data_seek, NULL)
	PHP_FE(sybase_fetch_field, NULL)
	PHP_FE(sybase_field_seek, NULL)
	PHP_FE(sybase_result, NULL)
	PHP_FE(sybase_affected_rows, NULL)
	PHP_FE(sybase_min_client_severity, NULL)
	PHP_FE(sybase_min_server_severity, NULL)
	PHP_FE(sybase_set_message_handler, NULL)
	PHP_FE(sybase_deadlock_retry_count, NULL)

#if !defined(PHP_WIN32) && !defined(HAVE_MSSQL)
	PHP_FALIAS(mssql_connect, sybase_connect, NULL)
	PHP_FALIAS(mssql_pconnect, sybase_pconnect, NULL)
	PHP_FALIAS(mssql_close, sybase_close, NULL)
	PHP_FALIAS(mssql_select_db, sybase_select_db, NULL)
	PHP_FALIAS(mssql_query, sybase_query, NULL)
	PHP_FALIAS(mssql_unbuffered_query, sybase_unbuffered_query, NULL)
	PHP_FALIAS(mssql_free_result, sybase_free_result, NULL)
	PHP_FALIAS(mssql_get_last_message, sybase_get_last_message, NULL)
	PHP_FALIAS(mssql_num_rows, sybase_num_rows, NULL)
	PHP_FALIAS(mssql_num_fields, sybase_num_fields, NULL)
	PHP_FALIAS(mssql_fetch_row, sybase_fetch_row, NULL)
	PHP_FALIAS(mssql_fetch_array, sybase_fetch_array, NULL)
	PHP_FALIAS(mssql_fetch_assoc, sybase_fetch_assoc, NULL)
	PHP_FALIAS(mssql_fetch_object, sybase_fetch_object, NULL)
	PHP_FALIAS(mssql_data_seek, sybase_data_seek, NULL)
	PHP_FALIAS(mssql_fetch_field, sybase_fetch_field, NULL)
	PHP_FALIAS(mssql_field_seek, sybase_field_seek, NULL)
	PHP_FALIAS(mssql_result, sybase_result, NULL)
	PHP_FALIAS(mssql_affected_rows, sybase_affected_rows, NULL)
	PHP_FALIAS(mssql_min_client_severity,   sybase_min_client_severity, NULL)
	PHP_FALIAS(mssql_min_server_severity, sybase_min_server_severity, NULL)
	PHP_FALIAS(mssql_set_message_handler, sybase_set_message_handler, NULL)
	PHP_FALIAS(mssql_deadlock_retry_count, sybase_deadlock_retry_count, NULL)
#endif

	{NULL, NULL, NULL}
};

zend_module_entry sybase_module_entry = {
	STANDARD_MODULE_HEADER,
	"sybase_ct", sybase_functions, PHP_MINIT(sybase), PHP_MSHUTDOWN(sybase), PHP_RINIT(sybase), PHP_RSHUTDOWN(sybase), PHP_MINFO(sybase), NO_VERSION_YET, STANDARD_MODULE_PROPERTIES
};

ZEND_DECLARE_MODULE_GLOBALS(sybase)
/* static CS_CONTEXT *context; */

#ifdef COMPILE_DL_SYBASE_CT
ZEND_GET_MODULE(sybase)
#endif

ZEND_DECLARE_MODULE_GLOBALS(sybase)

#define CHECK_LINK(link) { if (link==-1) { php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  A link to the server could not be established"); RETURN_FALSE; } }


static int _clean_invalid_results(zend_rsrc_list_entry *le TSRMLS_DC)
{
	if (Z_TYPE_P(le) == le_result) {
		sybase_link *sybase_ptr = ((sybase_result *) le->ptr)->sybase_ptr;

		if (!sybase_ptr->valid) {
			return 1;
		}
	}
	return 0;
}

static void _free_sybase_result(sybase_result *result)
{
	int i, j;

	if (result->data) {
		for (i = 0; i < (result->store ? result->num_rows : MIN(1, result->num_rows)); i++) {
			for (j=0; j<result->num_fields; j++) {
				zval_dtor(&result->data[i][j]);
			}
			efree(result->data[i]);
		}
		efree(result->data);
	}

	if (result->fields) {
		for (i=0; i<result->num_fields; i++) {
			STR_FREE(result->fields[i].name);
			STR_FREE(result->fields[i].column_source);
		}
		efree(result->fields);
	}

	efree(result);
}

/* Forward declaration */
static int php_sybase_finish_results (sybase_result *result);

static void php_free_sybase_result(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	sybase_result *result = (sybase_result *)rsrc->ptr;

	/* Check to see if we've read all rows */
	if (result->sybase_ptr && result->sybase_ptr->active_result_index) {
		if (result->sybase_ptr->cmd) {
			ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_ALL);
		}
		php_sybase_finish_results(result);
	}

	_free_sybase_result(result);
}

static void _close_sybase_link(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	sybase_link *sybase_ptr = (sybase_link *)rsrc->ptr;
	CS_INT con_status;

	sybase_ptr->valid = 0;
	if (sybase_ptr->callback_name != NULL) {
		zval_ptr_dtor(&sybase_ptr->callback_name);
		sybase_ptr->callback_name= NULL;
	}

	zend_hash_apply(&EG(regular_list), (apply_func_t) _clean_invalid_results TSRMLS_CC);

	/* Non-persistent connections will always be connected or we wouldn't
	 * get here, but since we want to check the death status anyway
	 * we might as well double-check the connect status.
	 */
	if (ct_con_props(sybase_ptr->connection, CS_GET, CS_CON_STATUS,
					 &con_status, CS_UNUSED, NULL)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to get connection status on close");
		/* Assume the worst. */
		con_status = CS_CONSTAT_CONNECTED | CS_CONSTAT_DEAD;
	}
	if (con_status & CS_CONSTAT_CONNECTED) {
		if ((con_status & CS_CONSTAT_DEAD) || ct_close(sybase_ptr->connection, CS_UNUSED)!=CS_SUCCEED) {
			ct_close(sybase_ptr->connection, CS_FORCE_CLOSE);
		}
	}

	ct_cmd_drop(sybase_ptr->cmd);
	ct_con_drop(sybase_ptr->connection);
	efree(sybase_ptr);
	SybCtG(num_links)--;
}


static void _close_sybase_plink(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	sybase_link *sybase_ptr = (sybase_link *)rsrc->ptr;
	CS_INT con_status;

	/* Persistent connections may have been closed before a failed
	 * reopen attempt.
	 */
	if (ct_con_props(sybase_ptr->connection, CS_GET, CS_CON_STATUS,
					 &con_status, CS_UNUSED, NULL)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to get connection status on close");
		/* Assume the worst. */
		con_status = CS_CONSTAT_CONNECTED | CS_CONSTAT_DEAD;
	}
	if (con_status & CS_CONSTAT_CONNECTED) {
		if ((con_status & CS_CONSTAT_DEAD) || ct_close(sybase_ptr->connection, CS_UNUSED)!=CS_SUCCEED) {
			ct_close(sybase_ptr->connection, CS_FORCE_CLOSE);
		}
	}

	ct_con_drop(sybase_ptr->connection);
	free(sybase_ptr);
	SybCtG(num_persistent)--;
	SybCtG(num_links)--;
}


static CS_RETCODE CS_PUBLIC _client_message_handler(CS_CONTEXT *context, CS_CONNECTION *connection, CS_CLIENTMSG *errmsg)
{
	TSRMLS_FETCH();

	if (CS_SEVERITY(errmsg->msgnumber) >= SybCtG(min_client_severity)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Client message:  %s (severity %d)", errmsg->msgstring, CS_SEVERITY(errmsg->msgnumber));
	}
	STR_FREE(SybCtG(server_message));
	SybCtG(server_message) = estrdup(errmsg->msgstring);


	/* If this is a timeout message, return CS_FAIL to cancel the
	 * operation and mark the connection as dead.
	 */
	if (CS_SEVERITY(errmsg->msgnumber) == CS_SV_RETRY_FAIL &&
		CS_NUMBER(errmsg->msgnumber) == 63 &&
		CS_ORIGIN(errmsg->msgnumber) == 2 &&
		CS_LAYER(errmsg->msgnumber) == 1)
	{
		return CS_FAIL;
	}

	return CS_SUCCEED;
}

static int _call_message_handler(zval *callback_name, CS_SERVERMSG *srvmsg TSRMLS_DC)
{
	int handled = 0;

	if (callback_name) {
		zval *msgnumber, *severity, *state, *line, *text, *retval = NULL;
		zval **args[5];

		MAKE_STD_ZVAL(msgnumber);
		ZVAL_LONG(msgnumber, srvmsg->msgnumber);
		args[0] = &msgnumber;

		MAKE_STD_ZVAL(severity);
		ZVAL_LONG(severity, srvmsg->severity);
		args[1] = &severity;

		MAKE_STD_ZVAL(state);
		ZVAL_LONG(state, srvmsg->state);
		args[2] = &state;

		MAKE_STD_ZVAL(line);
		ZVAL_LONG(line, srvmsg->line);
		args[3] = &line;

		MAKE_STD_ZVAL(text);	
		ZVAL_STRING(text, srvmsg->text, 1);
		args[4] = &text;
			
		if (call_user_function_ex(EG(function_table), NULL, callback_name, &retval, 5, args, 0, NULL TSRMLS_CC) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Cannot call the messagehandler %s", Z_STRVAL_P(callback_name));
		}

		if (retval) {
			handled= ((Z_TYPE_P(retval) != IS_BOOL) || (Z_BVAL_P(retval) != 0));
			zval_ptr_dtor(&retval);
		}

		zval_ptr_dtor(&msgnumber);
		zval_ptr_dtor(&severity);
		zval_ptr_dtor(&state);
		zval_ptr_dtor(&line);
		zval_ptr_dtor(&text);
	}

	return handled;
}

static CS_RETCODE CS_PUBLIC _server_message_handler(CS_CONTEXT *context, CS_CONNECTION *connection, CS_SERVERMSG *srvmsg)
{
	sybase_link *sybase;
	int handled = 0;
	TSRMLS_FETCH();

	/* Remember the last server message in any case */
	STR_FREE(SybCtG(server_message));
	SybCtG(server_message) = estrdup(srvmsg->text);

	/* Retrieve sybase link */
	if (ct_con_props(connection, CS_GET, CS_USERDATA, &sybase, CS_SIZEOF(sybase), NULL) != CS_SUCCEED) {
		sybase = NULL;
	}

	/* If this is a deadlock message, set the connection's deadlock flag
	 * so we will retry the request.  Sorry about the bare constant here,
	 * but it's not defined anywhere and it's a "well-known" number.
	 */
	if (sybase && (srvmsg->msgnumber == 1205)) {
		sybase->deadlock = 1;
	}

	/* Check mininum server severity level */
	if (srvmsg->severity < SybCtG(min_server_severity)) {
		return CS_SUCCEED;
	}

	/* Call global message handler */
	handled = handled | _call_message_handler(SybCtG(callback_name), srvmsg TSRMLS_CC);

	/* Call link specific message handler */
	if (sybase) {
		handled = handled | _call_message_handler(sybase->callback_name, srvmsg TSRMLS_CC);
	}

	/* Spit out a warning if neither of them has handled this message */
	if (!handled) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Server message:  %s (severity %d, procedure %s)",
				srvmsg->text, srvmsg->severity, ((srvmsg->proclen>0) ? srvmsg->proc : "N/A"));
	}

	return CS_SUCCEED;
}


PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("sybct.allow_persistent", "1", PHP_INI_SYSTEM, OnUpdateLong, allow_persistent, zend_sybase_globals, sybase_globals)
	STD_PHP_INI_ENTRY_EX("sybct.max_persistent", "-1", PHP_INI_SYSTEM, OnUpdateLong, max_persistent, zend_sybase_globals, sybase_globals, display_link_numbers)
	STD_PHP_INI_ENTRY_EX("sybct.max_links", "-1", PHP_INI_SYSTEM, OnUpdateLong, max_links, zend_sybase_globals, sybase_globals, display_link_numbers)
	STD_PHP_INI_ENTRY("sybct.min_server_severity", "10", PHP_INI_ALL, OnUpdateLong, min_server_severity, zend_sybase_globals, sybase_globals)
	STD_PHP_INI_ENTRY("sybct.min_client_severity", "10", PHP_INI_ALL, OnUpdateLong, min_client_severity, zend_sybase_globals, sybase_globals)
	STD_PHP_INI_ENTRY("sybct.login_timeout", "-1", PHP_INI_ALL, OnUpdateLong, login_timeout, zend_sybase_globals, sybase_globals)
	STD_PHP_INI_ENTRY("sybct.hostname", NULL, PHP_INI_ALL, OnUpdateString, hostname, zend_sybase_globals, sybase_globals)
	STD_PHP_INI_ENTRY_EX("sybct.deadlock_retry_count", "0", PHP_INI_ALL, OnUpdateLong, deadlock_retry_count, zend_sybase_globals, sybase_globals, display_link_numbers)
PHP_INI_END()


static void php_sybase_init_globals(zend_sybase_globals *sybase_globals)
{
	long opt;
	TSRMLS_FETCH();

	if (cs_ctx_alloc(CTLIB_VERSION, &sybase_globals->context)!=CS_SUCCEED || ct_init(sybase_globals->context, CTLIB_VERSION)!=CS_SUCCEED) {
		return;
	}

	/* Initialize message handlers */
	if (ct_callback(sybase_globals->context, NULL, CS_SET, CS_SERVERMSG_CB, (CS_VOID *)_server_message_handler)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to set server message handler");
	}

	if (ct_callback(sybase_globals->context, NULL, CS_SET, CS_CLIENTMSG_CB, (CS_VOID *)_client_message_handler)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to set client message handler");
	}

	/* Set datetime conversion format to "Nov  3 1998  8:06PM".
	 * This is the default format for the ct-lib that comes with
	 * Sybase ASE 11.5.1 for Solaris, but the Linux libraries that
	 * come with 11.0.3.3 default to "03/11/98" which is singularly
	 * useless.  This levels the playing field for all platforms.
	 */
	{
		CS_INT dt_convfmt = CS_DATES_SHORT;
		if (cs_dt_info(sybase_globals->context, CS_SET, NULL, CS_DT_CONVFMT, CS_UNUSED, &dt_convfmt, sizeof(dt_convfmt), NULL)!=CS_SUCCEED) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to set datetime conversion format");
		}
	}

	/* Set the timeout, which is per context and can't be set with 
	 * ct_con_props(), so set it globally from the config value if 
	 * requested.  The default is CS_NO_LIMIT.
	 * 
	 * Note that despite some noise in the documentation about using
	 * signals to implement timeouts, they are actually implemented
	 * by using poll() or select() on Solaris and Linux.
	 */
	if (cfg_get_long("sybct.timeout", &opt)==SUCCESS) {
		CS_INT cs_timeout = opt;
		if (ct_config(sybase_globals->context, CS_SET, CS_TIMEOUT, &cs_timeout, CS_UNUSED, NULL)!=CS_SUCCEED) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to update the timeout");
		}
	}

	sybase_globals->num_persistent=0;
	sybase_globals->callback_name = NULL;
}


static void php_sybase_destroy_globals(zend_sybase_globals *sybase_globals)
{
	ct_exit(sybase_globals->context, CS_UNUSED);
	cs_ctx_drop(sybase_globals->context);
}

PHP_MINIT_FUNCTION(sybase)
{
	ZEND_INIT_MODULE_GLOBALS(sybase, php_sybase_init_globals, php_sybase_destroy_globals);

	REGISTER_INI_ENTRIES();

	le_link = zend_register_list_destructors_ex(_close_sybase_link, NULL, "sybase-ct link", module_number);
	le_plink = zend_register_list_destructors_ex(NULL, _close_sybase_plink, "sybase-ct link persistent", module_number);
	le_result = zend_register_list_destructors_ex(php_free_sybase_result, NULL, "sybase-ct result", module_number);

	return SUCCESS;
}



PHP_RINIT_FUNCTION(sybase)
{
	SybCtG(default_link)=-1;
	SybCtG(num_links) = SybCtG(num_persistent);
	SybCtG(appname) = estrndup("PHP " PHP_VERSION, sizeof("PHP " PHP_VERSION));
	SybCtG(server_message) = STR_EMPTY_ALLOC();
	return SUCCESS;
}



PHP_MSHUTDOWN_FUNCTION(sybase)
{
	UNREGISTER_INI_ENTRIES();
#if 0
	ct_exit(context, CS_UNUSED);
	cs_ctx_drop(context);
#endif
	return SUCCESS;
}


PHP_RSHUTDOWN_FUNCTION(sybase)
{
	efree(SybCtG(appname));
	SybCtG(appname) = NULL;
	if (SybCtG(callback_name)) {
		zval_ptr_dtor(&SybCtG(callback_name));
		SybCtG(callback_name)= NULL;
	}
	STR_FREE(SybCtG(server_message));
	SybCtG(server_message) = NULL;
	return SUCCESS;
}


static int php_sybase_do_connect_internal(sybase_link *sybase, char *host, char *user, char *passwd, char *charset, char *appname)
{
	CS_LOCALE *tmp_locale;
	long packetsize;
	TSRMLS_FETCH();

	/* set a CS_CONNECTION record */
	if (ct_con_alloc(SybCtG(context), &sybase->connection)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to allocate connection record");
		return 0;
	}

	/* Note - this saves a copy of sybase, not a pointer to it. */
	if (ct_con_props(sybase->connection, CS_SET, CS_USERDATA, &sybase, CS_SIZEOF(sybase), NULL)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to set userdata");
		ct_con_drop(sybase->connection);
		return 0;
	}

	if (user) {
		ct_con_props(sybase->connection, CS_SET, CS_USERNAME, user, CS_NULLTERM, NULL);
	}
	if (passwd) {
		ct_con_props(sybase->connection, CS_SET, CS_PASSWORD, passwd, CS_NULLTERM, NULL);
	}
	if (appname) {
		ct_con_props(sybase->connection, CS_SET, CS_APPNAME, appname, CS_NULLTERM, NULL);
	} else { 
		ct_con_props(sybase->connection, CS_SET, CS_APPNAME, SybCtG(appname), CS_NULLTERM, NULL);
	}

	if (SybCtG(hostname)) {
		ct_con_props(sybase->connection, CS_SET, CS_HOSTNAME, SybCtG(hostname), CS_NULLTERM, NULL);
	}

	if (charset) {
		if (cs_loc_alloc(SybCtG(context), &tmp_locale)!=CS_SUCCEED) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase: Unable to allocate locale information.");
		} else {
			if (cs_locale(SybCtG(context), CS_SET, tmp_locale, CS_LC_ALL, NULL, CS_NULLTERM, NULL)!=CS_SUCCEED) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase: Unable to load default locale data.");
			} else {
				if (cs_locale(SybCtG(context), CS_SET, tmp_locale, CS_SYB_CHARSET, charset, CS_NULLTERM, NULL)!=CS_SUCCEED) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase: Unable to update character set.");
				} else {
					if (ct_con_props(sybase->connection, CS_SET, CS_LOC_PROP, tmp_locale, CS_UNUSED, NULL)!=CS_SUCCEED) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase: Unable to update connection properties.");
					}
				}
			}
		}
	}
	
	if (cfg_get_long("sybct.packet_size", &packetsize) == SUCCESS) {
		if (ct_con_props(sybase->connection, CS_SET, CS_PACKETSIZE, (CS_VOID *)&packetsize, CS_UNUSED, NULL) != CS_SUCCEED) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase: Unable to update connection packetsize.");
		}
	}

	/* Set the login timeout. Actually, the login timeout is per context
	 * and not per connection, but we will update the context here to 
	 * allow for code such as the following:
	 * 
	 *   ini_set('sybct.login_timeout', $timeout);
	 *   sybase_connect(...)
	 * 
	 * Note that preceding calls to sybase_connect() will now use the 
	 * updated value and not the default one!
	 * 
  	 * The default value for CS_LOGIN_TIMEOUT is 60 (1 minute).
	 */
	if (SybCtG(login_timeout) != -1) {
		CS_INT cs_login_timeout = SybCtG(login_timeout);
		if (ct_config(SybCtG(context), CS_SET, CS_LOGIN_TIMEOUT, &cs_login_timeout, CS_UNUSED, NULL)!=CS_SUCCEED) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to update the login timeout");
		}
	}

	sybase->valid = 1;
	sybase->dead = 0;
	sybase->active_result_index = 0;
	sybase->callback_name = NULL;

	/* create the link */
	if (ct_connect(sybase->connection, host, CS_NULLTERM)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to connect");
		ct_con_drop(sybase->connection);
		return 0;
	}

	if (ct_cmd_alloc(sybase->connection, &sybase->cmd)!=CS_SUCCEED) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to allocate command record");
		ct_close(sybase->connection, CS_UNUSED);
		ct_con_drop(sybase->connection);
		return 0;
	}

	return 1;
}


static void php_sybase_do_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *user, *passwd, *host, *charset, *appname;
	char *hashed_details;
	int hashed_details_length;
	sybase_link *sybase_ptr;

	switch(ZEND_NUM_ARGS()) {
		case 0: /* defaults */
			host=user=passwd=charset=appname=NULL;
			hashed_details_length=6+5;
			hashed_details = (char *) emalloc(hashed_details_length+1);
			strcpy(hashed_details, "sybase_____");
			break;
		case 1: {
				zval **yyhost;

				if (zend_get_parameters_ex(1, &yyhost) == FAILURE) {
					RETURN_FALSE;
				}
				convert_to_string_ex(yyhost);
				host = Z_STRVAL_PP(yyhost);
				user=passwd=charset=appname=NULL;
				hashed_details_length = Z_STRLEN_PP(yyhost)+6+5;
				hashed_details = (char *) emalloc(hashed_details_length+1);
				sprintf(hashed_details, "sybase_%s____", Z_STRVAL_PP(yyhost));
			}
			break;
		case 2: {
				zval **yyhost, **yyuser;

				if (zend_get_parameters_ex(2, &yyhost, &yyuser) == FAILURE) {
					RETURN_FALSE;
				}
				convert_to_string_ex(yyhost);
				convert_to_string_ex(yyuser);
				host = Z_STRVAL_PP(yyhost);
				user = Z_STRVAL_PP(yyuser);
				passwd=charset=appname=NULL;
				hashed_details_length = Z_STRLEN_PP(yyhost)+Z_STRLEN_PP(yyuser)+6+5;
				hashed_details = (char *) emalloc(hashed_details_length+1);
				sprintf(hashed_details, "sybase_%s_%s___", Z_STRVAL_PP(yyhost), Z_STRVAL_PP(yyuser));
			}
			break;
		case 3: {
				zval **yyhost, **yyuser, **yypasswd;

				if (zend_get_parameters_ex(3, &yyhost, &yyuser, &yypasswd) == FAILURE) {
					RETURN_FALSE;
				}
				convert_to_string_ex(yyhost);
				convert_to_string_ex(yyuser);
				convert_to_string_ex(yypasswd);
				host = Z_STRVAL_PP(yyhost);
				user = Z_STRVAL_PP(yyuser);
				passwd = Z_STRVAL_PP(yypasswd);
				charset=appname=NULL;
				hashed_details_length = Z_STRLEN_PP(yyhost)+Z_STRLEN_PP(yyuser)+Z_STRLEN_PP(yypasswd)+6+5;
				hashed_details = (char *) emalloc(hashed_details_length+1);
				sprintf(hashed_details, "sybase_%s_%s_%s__", Z_STRVAL_PP(yyhost), Z_STRVAL_PP(yyuser), Z_STRVAL_PP(yypasswd));
			}
			break;
		case 4: {
				zval **yyhost, **yyuser, **yypasswd, **yycharset;

				if (zend_get_parameters_ex(4, &yyhost, &yyuser, &yypasswd, &yycharset) == FAILURE) {
					RETURN_FALSE;
				}
				convert_to_string_ex(yyhost);
				convert_to_string_ex(yyuser);
				convert_to_string_ex(yypasswd);
				convert_to_string_ex(yycharset);
				host = Z_STRVAL_PP(yyhost);
				user = Z_STRVAL_PP(yyuser);
				passwd = Z_STRVAL_PP(yypasswd);
				charset = Z_STRVAL_PP(yycharset);
				appname=NULL;
				hashed_details_length = Z_STRLEN_PP(yyhost)+Z_STRLEN_PP(yyuser)+Z_STRLEN_PP(yypasswd)+Z_STRLEN_PP(yycharset)+6+5;
				hashed_details = (char *) emalloc(hashed_details_length+1);
				sprintf(hashed_details, "sybase_%s_%s_%s_%s_", Z_STRVAL_PP(yyhost), Z_STRVAL_PP(yyuser), Z_STRVAL_PP(yypasswd), Z_STRVAL_PP(yycharset));
			}
			break;
		case 5: {
				zval **yyhost, **yyuser, **yypasswd, **yycharset, **yyappname;

				if (zend_get_parameters_ex(5, &yyhost, &yyuser, &yypasswd, &yycharset, &yyappname) == FAILURE) {
					RETURN_FALSE;
				}
				convert_to_string_ex(yyhost);
				convert_to_string_ex(yyuser);
				convert_to_string_ex(yypasswd);
				convert_to_string_ex(yycharset);
				convert_to_string_ex(yyappname);
				host = Z_STRVAL_PP(yyhost);
				user = Z_STRVAL_PP(yyuser);
				passwd = Z_STRVAL_PP(yypasswd);
				charset = Z_STRVAL_PP(yycharset);
				appname = Z_STRVAL_PP(yyappname);
				hashed_details_length = Z_STRLEN_PP(yyhost)+Z_STRLEN_PP(yyuser)+Z_STRLEN_PP(yypasswd)+Z_STRLEN_PP(yycharset)+Z_STRLEN_PP(yyappname)+6+5;
				hashed_details = (char *) emalloc(hashed_details_length+1);
				sprintf(hashed_details, "sybase_%s_%s_%s_%s_%s", Z_STRVAL_PP(yyhost), Z_STRVAL_PP(yyuser), Z_STRVAL_PP(yypasswd), Z_STRVAL_PP(yycharset), Z_STRVAL_PP(yyappname));
			}
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	if (!SybCtG(allow_persistent)) {
		persistent=0;
	}
	if (persistent) {
		zend_rsrc_list_entry *le;

		/* try to find if we already have this link in our persistent list */
		if (zend_hash_find(&EG(persistent_list), hashed_details, hashed_details_length+1, (void **) &le)==FAILURE) {  /* we don't */
			zend_rsrc_list_entry new_le;

			if (SybCtG(max_links)!=-1 && SybCtG(num_links)>=SybCtG(max_links)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Too many open links (%ld)", SybCtG(num_links));
				efree(hashed_details);
				RETURN_FALSE;
			}
			if (SybCtG(max_persistent)!=-1 && SybCtG(num_persistent)>=SybCtG(max_persistent)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Too many open persistent links (%ld)", SybCtG(num_persistent));
				efree(hashed_details);
				RETURN_FALSE;
			}

			sybase_ptr = (sybase_link *) malloc(sizeof(sybase_link));
			if (!php_sybase_do_connect_internal(sybase_ptr, host, user, passwd, charset, appname)) {
				free(sybase_ptr);
				efree(hashed_details);
				RETURN_FALSE;
			}

			/* hash it up */
			Z_TYPE(new_le) = le_plink;
			new_le.ptr = sybase_ptr;
			if (zend_hash_update(&EG(persistent_list), hashed_details, hashed_details_length+1, (void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
				ct_close(sybase_ptr->connection, CS_UNUSED);
				ct_con_drop(sybase_ptr->connection);
				free(sybase_ptr);
				efree(hashed_details);
				RETURN_FALSE;
			}
			SybCtG(num_persistent)++;
			SybCtG(num_links)++;
		} else {  /* we do */
			CS_INT con_status;

			if (Z_TYPE_P(le) != le_plink) {
				efree(hashed_details);
				RETURN_FALSE;
			}

			sybase_ptr = (sybase_link *) le->ptr;

			/* If the link has died, close it and overwrite it with a new one. */

			if (ct_con_props(sybase_ptr->connection, CS_GET, CS_CON_STATUS,
							 &con_status, CS_UNUSED, NULL)!=CS_SUCCEED) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Unable to get connection status");
				efree(hashed_details);
				RETURN_FALSE;
			}
			if (!(con_status & CS_CONSTAT_CONNECTED) || (con_status & CS_CONSTAT_DEAD) || sybase_ptr->dead) {
				sybase_link sybase;

				if (con_status & CS_CONSTAT_CONNECTED) {
					ct_close(sybase_ptr->connection, CS_FORCE_CLOSE);
				}
				/* Create a new connection, then replace the old
				 * connection.  If we fail to create a new connection,
				 * put the old one back so there will be a connection,
				 * even if it is a non-functional one.  This is because
				 * code may still be holding an id for this connection
				 * so we can't free the CS_CONNECTION.
				 * (This is actually totally hokey, it would be better
				 * to just ct_con_drop() the connection and set
				 * sybase_ptr->connection to NULL, then test it for
				 * NULL before trying to use it elsewhere . . .)
				 */
				memcpy(&sybase, sybase_ptr, sizeof(sybase_link));
				if (!php_sybase_do_connect_internal(sybase_ptr, host, user, passwd, charset, appname)) {
					memcpy(sybase_ptr, &sybase, sizeof(sybase_link));
					efree(hashed_details);
					RETURN_FALSE;
				}
				ct_con_drop(sybase.connection); /* drop old connection */
			}
		}
		ZEND_REGISTER_RESOURCE(return_value, sybase_ptr, le_plink);
	} else { /* non persistent */
		zend_rsrc_list_entry *index_ptr, new_index_ptr;

		/* first we check the hash for the hashed_details key.  if it exists,
		 * it should point us to the right offset where the actual sybase link sits.
		 * if it doesn't, open a new sybase link, add it to the resource list,
		 * and add a pointer to it with hashed_details as the key.
		 */
		if (zend_hash_find(&EG(regular_list), hashed_details, hashed_details_length+1, (void **) &index_ptr)==SUCCESS) {
			int type, link;
			void *ptr;

			if (Z_TYPE_P(index_ptr) != le_index_ptr) {
				efree(hashed_details);
				RETURN_FALSE;
			}
			link = (int) index_ptr->ptr;
			ptr = zend_list_find(link, &type);   /* check if the link is still there */
			if (ptr && (type==le_link || type==le_plink)) {
				zend_list_addref(link);
				Z_LVAL_P(return_value) = SybCtG(default_link) = link;
				Z_TYPE_P(return_value) = IS_RESOURCE;
				efree(hashed_details);
				return;
			} else {
				zend_hash_del(&EG(regular_list), hashed_details, hashed_details_length+1);
			}
		}
		if (SybCtG(max_links)!=-1 && SybCtG(num_links)>=SybCtG(max_links)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Too many open links (%ld)", SybCtG(num_links));
			efree(hashed_details);
			RETURN_FALSE;
		}

		sybase_ptr = (sybase_link *) emalloc(sizeof(sybase_link));
		if (!php_sybase_do_connect_internal(sybase_ptr, host, user, passwd, charset, appname)) {
			efree(sybase_ptr);
			efree(hashed_details);
			RETURN_FALSE;
		}

		/* add it to the list */
		ZEND_REGISTER_RESOURCE(return_value, sybase_ptr, le_link);

		/* add it to the hash */
		new_index_ptr.ptr = (void *) Z_LVAL_P(return_value);
		Z_TYPE(new_index_ptr) = le_index_ptr;
		if (zend_hash_update(&EG(regular_list), hashed_details, hashed_details_length+1, (void *) &new_index_ptr, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
			ct_close(sybase_ptr->connection, CS_UNUSED);
			ct_con_drop(sybase_ptr->connection);
			efree(sybase_ptr);
			efree(hashed_details);
			RETURN_FALSE;
		}
		SybCtG(num_links)++;
	}
	efree(hashed_details);
	SybCtG(default_link)=Z_LVAL_P(return_value);
	zend_list_addref(SybCtG(default_link));
}


static int php_sybase_get_default_link(INTERNAL_FUNCTION_PARAMETERS)
{
	if (SybCtG(default_link)==-1) { /* no link opened yet, implicitly open one */
		ht = 0;
		php_sybase_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
	}
	return SybCtG(default_link);
}


/* {{{ proto int sybase_connect([string host [, string user [, string password [, string charset [, string appname]]]]])
   Open Sybase server connection */
PHP_FUNCTION(sybase_connect)
{
	php_sybase_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}

/* }}} */

/* {{{ proto int sybase_pconnect([string host [, string user [, string password [, string charset [, string appname]]]]])
   Open persistent Sybase connection */
PHP_FUNCTION(sybase_pconnect)
{
	php_sybase_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}

/* }}} */


/* {{{ proto bool sybase_close([int link_id])
   Close Sybase connection */
PHP_FUNCTION(sybase_close)
{
	zval **sybase_link_index = 0;
	int id;
	sybase_link *sybase_ptr;

	switch (ZEND_NUM_ARGS()) {
		case 0:
			id = SybCtG(default_link);
			break;
		case 1:
			if (zend_get_parameters_ex(1, &sybase_link_index) == FAILURE) {
				RETURN_FALSE;
			}
			id = -1;
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	ZEND_FETCH_RESOURCE2(sybase_ptr, sybase_link *, sybase_link_index, id, "Sybase-Link", le_link, le_plink);

	if (id == -1) { /* explicit resource number */
		zend_list_delete(Z_RESVAL_PP(sybase_link_index));
	}

	if (id != -1 || (sybase_link_index && Z_RESVAL_PP(sybase_link_index)==SybCtG(default_link))) {
		zend_list_delete(SybCtG(default_link));
		SybCtG(default_link) = -1;
	}

	RETURN_TRUE;
}

/* }}} */


static int exec_cmd(sybase_link *sybase_ptr, char *cmdbuf)
{
	CS_RETCODE retcode;
	CS_INT restype;
	int failure=0;

	/* Fail if we already marked this connection dead. */

	if (sybase_ptr->dead) {
		return FAILURE;
	}

	/*
	 ** Get a command handle, store the command string in it, and
	 ** send it to the server.
	 */

	if (ct_command(sybase_ptr->cmd, CS_LANG_CMD, cmdbuf, CS_NULLTERM, CS_UNUSED)!=CS_SUCCEED) {
		sybase_ptr->dead = 1;
		return FAILURE;
	}
	if (ct_send(sybase_ptr->cmd)!=CS_SUCCEED) {
		sybase_ptr->dead = 1;
		return FAILURE;
	}

	while ((retcode = ct_results(sybase_ptr->cmd, &restype))==CS_SUCCEED) {
		switch ((int) restype) {
			case CS_CMD_SUCCEED:
			case CS_CMD_DONE:
				break;

			case CS_CMD_FAIL:
				failure=1;
				break;

			case CS_STATUS_RESULT:
				ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_CURRENT);
				break;

			default:
				failure=1;
				break;
		}
		if (failure) {
			ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
			return FAILURE;
		}
	}

	switch (retcode) {
		case CS_END_RESULTS:
			return SUCCESS;
			break;

		case CS_FAIL:
			/* Hopefully this either cleans up the connection, or the
			 * connection ends up marked dead so it will be reopened
			 * if it is persistent.  We may want to do
			 * ct_close(CS_FORCE_CLOSE) if ct_cancel() fails; see the
			 * doc for ct_results()==CS_FAIL.
			 */
			ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
			/* Don't take chances with the vagaries of ct-lib.  Mark it
			 * dead ourselves.
			 */
			sybase_ptr->dead = 1;
			return FAILURE;

		default:
			return FAILURE;
	}
}


/* {{{ proto bool sybase_select_db(string database [, int link_id])
   Select Sybase database */
PHP_FUNCTION(sybase_select_db)
{
	zval **db, **sybase_link_index;
	int id;
	char *cmdbuf;
	sybase_link  *sybase_ptr;

	switch(ZEND_NUM_ARGS()) {
		case 1:
			if (zend_get_parameters_ex(1, &db) == FAILURE) {
				RETURN_FALSE;
			}
			id = php_sybase_get_default_link(INTERNAL_FUNCTION_PARAM_PASSTHRU);
			CHECK_LINK(id);
			break;
		case 2:
			if (zend_get_parameters_ex(2, &db, &sybase_link_index) == FAILURE) {
				RETURN_FALSE;
			}
			id = -1;
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	ZEND_FETCH_RESOURCE2(sybase_ptr, sybase_link *, sybase_link_index, id, "Sybase-Link", le_link, le_plink);

	convert_to_string_ex(db);
	cmdbuf = (char *) emalloc(sizeof("use ")+Z_STRLEN_PP(db)+1);
	sprintf(cmdbuf, "use %s", Z_STRVAL_PP(db)); /* SAFE */

	if (exec_cmd(sybase_ptr, cmdbuf)==FAILURE) {
		efree(cmdbuf);
		RETURN_FALSE;
	} else {
		efree(cmdbuf);
		RETURN_TRUE;
	}
}

/* }}} */

static int php_sybase_finish_results (sybase_result *result) 
{
	int i, fail;
	CS_RETCODE retcode;
	CS_INT restype;
	TSRMLS_FETCH();
	
	efree(result->datafmt);
	efree(result->lengths);
	efree(result->indicators);
	efree(result->numerics);
	efree(result->types);
	for (i=0; i<result->num_fields; i++) {
		efree(result->tmp_buffer[i]);
	}
	efree(result->tmp_buffer);

	/* Indicate we have read all rows */
	result->sybase_ptr->active_result_index= 0;

	/* The only restype we should get now is CS_CMD_DONE, possibly
	 * followed by a CS_STATUS_RESULT/CS_CMD_SUCCEED/CS_CMD_DONE
	 * sequence if the command was a stored procedure call.  But we
	 * still need to read and discard unexpected results.  We might
	 * want to return a failure in this case because the application
	 * won't be getting all the results it asked for.
	 */
	fail = 0;
	while ((retcode = ct_results(result->sybase_ptr->cmd, &restype))==CS_SUCCEED) {
		switch ((int) restype) {
			case CS_CMD_SUCCEED:
			case CS_CMD_DONE:
				break;

			case CS_CMD_FAIL:
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Command failed, cancelling rest");
				ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_ALL);
				fail = 1;
				break;

			case CS_COMPUTE_RESULT:
			case CS_CURSOR_RESULT:
			case CS_PARAM_RESULT:
			case CS_ROW_RESULT:
				/* Unexpected results, cancel them. */
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Sybase:  Unexpected results, cancelling current");
				ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_CURRENT);
				break;

			case CS_STATUS_RESULT:
				/* Status result from a stored procedure, cancel it but do not tell user */
				ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_CURRENT);
				break;

			default:
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Sybase:  Unexpected results, cancelling all");
				ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_ALL);
				break;
		}

		if (fail) {
			break;
		}
	}

	switch (retcode) {
		case CS_END_RESULTS:
			/* Normal. */
			break;

		case CS_FAIL:
			/* Hopefully this either cleans up the connection, or the
			 * connection ends up marked dead so it will be reopened
			 * if it is persistent.  We may want to do
			 * ct_close(CS_FORCE_CLOSE) if ct_cancel() fails; see the
			 * doc for ct_results()==CS_FAIL.
			 */
			ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_ALL);
			/* Don't take chances with the vagaries of ct-lib.  Mark it
			 * dead ourselves.
			 */
			result->sybase_ptr->dead = 1;
			
		case CS_CANCELED:
		default:
			_free_sybase_result(result);
			result = NULL;
			retcode = CS_FAIL;
			break;
	}

	return retcode;
}

#define RETURN_DOUBLE_VAL(result, buf, length)          \
	if ((length - 1) <= EG(precision)) {                \
		errno = 0;                                      \
		Z_DVAL(result) = strtod(buf, NULL);             \
		if (errno != ERANGE) {                          \
			Z_TYPE(result) = IS_DOUBLE;                 \
		} else {                                        \
			ZVAL_STRINGL(&result, buf, length- 1, 1);   \
		}                                               \
	} else {                                            \
		ZVAL_STRINGL(&result, buf, length- 1, 1);       \
	}

static int php_sybase_fetch_result_row (sybase_result *result, int numrows)
{
	int i, j;
	CS_INT retcode;
	TSRMLS_FETCH();
	
	/* We've already fetched everything */
	if (result->last_retcode == CS_END_DATA || result->last_retcode == CS_END_RESULTS) {
		return result->last_retcode;
	}
	
	if (numrows!=-1) numrows+= result->num_rows;
	while ((retcode=ct_fetch(result->sybase_ptr->cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, NULL))==CS_SUCCEED
			|| retcode==CS_ROW_FAIL) {
		/*
		if (retcode==CS_ROW_FAIL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Error reading row %d", result->num_rows);
		}
		*/
		
		result->num_rows++;
		i= result->store ? result->num_rows- 1 : 0;
		if (i >= result->blocks_initialized*SYBASE_ROWS_BLOCK) {
			result->data = (zval **) erealloc(result->data, sizeof(zval *)*SYBASE_ROWS_BLOCK*(++result->blocks_initialized));
		}
		if (result->store || 1 == result->num_rows) {
			result->data[i] = (zval *) safe_emalloc(sizeof(zval), result->num_fields, 0);
		}

		for (j = 0; j < result->num_fields; j++) {

			/* If we are in non-storing mode, free the previous result */
			if (!result->store && result->num_rows > 1 && Z_TYPE(result->data[i][j]) == IS_STRING) {
				efree(Z_STRVAL(result->data[i][j]));
			}

			if (result->indicators[j] == -1) { /* null value */
				ZVAL_NULL(&result->data[i][j]);
			} else {
				switch (result->numerics[j]) {
					case 1: {
						/* This indicates a long */
						ZVAL_LONG(&result->data[i][j], strtol(result->tmp_buffer[j], NULL, 10));
						break;
					}
					
					case 2: {
						/* This indicates a float */
						RETURN_DOUBLE_VAL(result->data[i][j], result->tmp_buffer[j], result->lengths[j]); 
						break;
					}

					case 3: {
						/* This indicates either a long or a float, which ever fits */
						errno = 0;
						Z_LVAL(result->data[i][j]) = strtol(result->tmp_buffer[j], NULL, 10);
						if (errno == ERANGE) {
						
							/* An overflow occurred, so try to fit it into a double */
							RETURN_DOUBLE_VAL(result->data[i][j], result->tmp_buffer[j], result->lengths[j]); 
							break;
						}
						Z_TYPE(result->data[i][j]) = IS_LONG;
						break;
					}
					
					default: {
						/* This indicates anything else, return it as string */
						ZVAL_STRINGL(&result->data[i][j], result->tmp_buffer[j], result->lengths[j]- 1, 1);
						break;
					}	   
				}
			}
		}
		if (numrows!=-1 && result->num_rows>=numrows) break;
	}
	
	result->last_retcode= retcode;
	switch (retcode) {
		case CS_END_DATA:
			retcode = php_sybase_finish_results(result);
			break;
			
		case CS_ROW_FAIL:
		case CS_SUCCEED:
			break;
			
		default:
			_free_sybase_result(result);
			result = NULL;
			retcode = CS_FAIL;		/* Just to be sure */
			break;
	}
	
	return retcode;
}

static sybase_result * php_sybase_fetch_result_set (sybase_link *sybase_ptr, int buffered, int store)
{
	int num_fields;
	sybase_result *result;
	int i, j;
	CS_INT retcode;

	/* The following (if unbuffered) is more or less the equivalent of mysql_store_result().
	 * fetch all rows from the server into the row buffer, thus:
	 * 1)  Being able to fire up another query without explicitly reading all rows
	 * 2)  Having numrows accessible
	 */
	if (ct_res_info(sybase_ptr->cmd, CS_NUMDATA, &num_fields, CS_UNUSED, NULL)!=CS_SUCCEED) {
		return NULL;
	}
	
	result = (sybase_result *) emalloc(sizeof(sybase_result));
	result->data = (zval **) safe_emalloc(sizeof(zval *), SYBASE_ROWS_BLOCK, 0);
	result->fields = NULL;
	result->sybase_ptr = sybase_ptr;
	result->cur_field=result->cur_row=result->num_rows=0;
	result->num_fields = num_fields;
	result->last_retcode = 0;
	result->store= store;
	result->blocks_initialized= 1;
	result->tmp_buffer = (char **) safe_emalloc(sizeof(char *), num_fields, 0);
	result->lengths = (CS_INT *) safe_emalloc(sizeof(CS_INT), num_fields, 0);
	result->indicators = (CS_SMALLINT *) safe_emalloc(sizeof(CS_INT), num_fields, 0);
	result->datafmt = (CS_DATAFMT *) safe_emalloc(sizeof(CS_DATAFMT), num_fields, 0);
	result->numerics = (unsigned char *) safe_emalloc(sizeof(unsigned char), num_fields, 0);
	result->types = (CS_INT *) safe_emalloc(sizeof(CS_INT), num_fields, 0);
	
	for (i=0; i<num_fields; i++) {
		ct_describe(sybase_ptr->cmd, i+1, &result->datafmt[i]);
		result->types[i] = result->datafmt[i].datatype;
		switch (result->datafmt[i].datatype) {
			case CS_CHAR_TYPE:
			case CS_VARCHAR_TYPE:
			case CS_TEXT_TYPE:
			case CS_IMAGE_TYPE:
				result->datafmt[i].maxlength++;
				result->numerics[i] = 0;
				break;
			case CS_BINARY_TYPE:
			case CS_VARBINARY_TYPE:
				result->datafmt[i].maxlength *= 2;
				result->datafmt[i].maxlength++;
				result->numerics[i] = 0;
				break;
			case CS_BIT_TYPE:
			case CS_TINYINT_TYPE:
				result->datafmt[i].maxlength = 4;
				result->numerics[i] = 1;
				break;
			case CS_SMALLINT_TYPE:
				result->datafmt[i].maxlength = 7;
				result->numerics[i] = 1;
				break;
			case CS_INT_TYPE:
				result->datafmt[i].maxlength = 12;
				result->numerics[i] = 1;
				break;
			case CS_REAL_TYPE:
			case CS_FLOAT_TYPE:
				result->datafmt[i].maxlength = 24;
				result->numerics[i] = 2;
				break;
			case CS_MONEY_TYPE:
			case CS_MONEY4_TYPE:
				result->datafmt[i].maxlength = 24;
				result->numerics[i] = 2;
				break;
			case CS_DATETIME_TYPE:
			case CS_DATETIME4_TYPE:
				result->datafmt[i].maxlength = 30;
				result->numerics[i] = 0;
				break;
			case CS_NUMERIC_TYPE:
			case CS_DECIMAL_TYPE:
				result->datafmt[i].maxlength = result->datafmt[i].precision + 3;
				/* numeric(10) vs numeric(10, 1) */
				result->numerics[i] = (result->datafmt[i].scale == 0) ? 3 : 2;
				break;
			default:
				result->datafmt[i].maxlength++;
				result->numerics[i] = 0;
				break;
		}
		result->tmp_buffer[i] = (char *)emalloc(result->datafmt[i].maxlength);
		result->datafmt[i].datatype = CS_CHAR_TYPE;
		result->datafmt[i].format = CS_FMT_NULLTERM;
		ct_bind(sybase_ptr->cmd, i+1, &result->datafmt[i], result->tmp_buffer[i], &result->lengths[i], &result->indicators[i]);
	}

	result->fields = (sybase_field *) safe_emalloc(sizeof(sybase_field), num_fields, 0);
	j=0;
	for (i=0; i<num_fields; i++) {
		char computed_buf[16];

		if (result->datafmt[i].namelen>0) {
			result->fields[i].name = estrndup(result->datafmt[i].name, result->datafmt[i].namelen);
		} else {
			if (j>0) {
				snprintf(computed_buf, 16, "computed%d", j);
			} else {
				strcpy(computed_buf, "computed");
			}
			result->fields[i].name = estrdup(computed_buf);
			j++;
		}
		result->fields[i].column_source = STR_EMPTY_ALLOC();
		result->fields[i].max_length = result->datafmt[i].maxlength-1;
		result->fields[i].numeric = result->numerics[i];
		Z_TYPE(result->fields[i]) = result->types[i];
	}
	
	if (buffered) {
		retcode = CS_SUCCEED;
	} else {
		if ((retcode = php_sybase_fetch_result_row(result, -1)) == CS_FAIL) {
			return NULL;
		}
	}

	result->last_retcode = retcode;
	return result;
}

static void php_sybase_query (INTERNAL_FUNCTION_PARAMETERS, int buffered)
{
	zval **query, **sybase_link_index=NULL;
	zval **store_mode= NULL;
	int id, deadlock_count, store;
	sybase_link *sybase_ptr;
	sybase_result *result;
	CS_INT restype;
	CS_RETCODE retcode;
	enum {
		Q_RESULT,				/* Success with results. */
		Q_SUCCESS,				/* Success but no results. */
		Q_FAILURE,				/* Failure, no results. */
	} status;

	store= 1;
	switch(ZEND_NUM_ARGS()) {
		case 1:
			if (zend_get_parameters_ex(1, &query)==FAILURE) {
				RETURN_FALSE;
			}
			id = SybCtG(default_link);
			break;
		case 2:
			if (zend_get_parameters_ex(2, &query, &sybase_link_index)==FAILURE) {
				RETURN_FALSE;
			}
			id = -1;
			break;
		case 3:
			if (zend_get_parameters_ex(3, &query, &sybase_link_index, &store_mode)==FAILURE) {
				RETURN_FALSE;
			}
			if (!buffered) {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "cannot use non-storing mode with buffered queries");
				store = 1;
			} else {
				convert_to_long_ex(store_mode);
				store= (Z_LVAL_PP(store_mode) != 0);
			}
			id = -1;
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	ZEND_FETCH_RESOURCE2(sybase_ptr, sybase_link *, sybase_link_index, id, "Sybase-Link", le_link, le_plink);

	convert_to_string_ex(query);

	/* Fail if we already marked this connection dead. */
	if (sybase_ptr->dead) {
		RETURN_FALSE;
	}
	
	/* Check to see if a previous sybase_unbuffered_query has read all rows */
	if (sybase_ptr->active_result_index) {
		zval *tmp = NULL;
		
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "called without first fetching all rows from a previous unbuffered query");
		if (sybase_ptr->cmd) {
			ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
		}
		
		/* Get the resultset and free it */
		ALLOC_ZVAL(tmp);
		Z_LVAL_P(tmp)= sybase_ptr->active_result_index;
		Z_TYPE_P(tmp)= IS_RESOURCE;
		INIT_PZVAL(tmp);
		ZEND_FETCH_RESOURCE(result, sybase_result *, &tmp, -1, "Sybase result", le_result);
		
		/* Causes the following segfault:
		   Program received signal SIGSEGV, Segmentation fault.
		   0x8144380 in _efree (ptr=0x81fe024, __zend_filename=0x81841a0 "php4/ext/sybase_ct/php_sybase_ct.c", 
		   __zend_lineno=946, __zend_orig_filename=0x0, __zend_orig_lineno=0) at php4/Zend/zend_alloc.c:229
		   php4/Zend/zend_alloc.c:229:7284:beg:0x8144380
		*/
		#if O_TIMM
		if (result) {
			php_sybase_finish_results(result);
		}
		#endif
		
		zval_ptr_dtor(&tmp);
		zend_list_delete(sybase_ptr->active_result_index);
		sybase_ptr->active_result_index= 0;
	}

	/* Repeat until we don't deadlock. */
	deadlock_count= 0;
	for (;;) {
		result = NULL;
		sybase_ptr->deadlock = 0;
		sybase_ptr->affected_rows = 0;

		/* On Solaris 11.5, ct_command() can be moved outside the
		 * loop, but not on Linux 11.0.
		 */
		if (ct_command(sybase_ptr->cmd, CS_LANG_CMD, Z_STRVAL_PP(query), CS_NULLTERM, CS_UNUSED)!=CS_SUCCEED) {
			/* If this didn't work, the connection is screwed but
			 * ct-lib might not set CS_CONSTAT_DEAD.  So set our own
			 * flag.  This happens sometimes when the database is restarted
			 * and/or its machine is rebooted, and ct_command() returns
			 * CS_BUSY for some reason.
			 */
			sybase_ptr->dead = 1;
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Connection is dead");
			RETURN_FALSE;
		}

		if (ct_send(sybase_ptr->cmd)!=CS_SUCCEED) {
			ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
			sybase_ptr->dead = 1;
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Cannot send command");
			RETURN_FALSE;
		}

		/* Use the first result set or succeed/fail status and discard the
		 * others.  Applications really shouldn't be making calls that
		 * return multiple result sets, but if they do then we need to
		 * properly read or cancel them or the connection will become
		 * unusable.
		 */
		if (ct_results(sybase_ptr->cmd, &restype)!=CS_SUCCEED) {
			ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
			sybase_ptr->dead = 1;
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Cannot read results");
			RETURN_FALSE;
		}
		switch ((int) restype) {
			case CS_CMD_FAIL:
			default:
				status = Q_FAILURE;
				break;
			case CS_CMD_SUCCEED:
			case CS_CMD_DONE: {
					CS_INT row_count;
					if (ct_res_info(sybase_ptr->cmd, CS_ROW_COUNT, &row_count, CS_UNUSED, NULL)==CS_SUCCEED) {
						sybase_ptr->affected_rows = (long)row_count;
					}
				}
				/* Fall through */
			case CS_COMPUTEFMT_RESULT:
			case CS_ROWFMT_RESULT:
			case CS_DESCRIBE_RESULT:
			case CS_MSG_RESULT:
				buffered= 0;				/* These queries have no need for buffering */
				status = Q_SUCCESS;
				break;
			case CS_COMPUTE_RESULT:
			case CS_CURSOR_RESULT:
			case CS_PARAM_RESULT:
			case CS_ROW_RESULT:
			case CS_STATUS_RESULT:
				result = php_sybase_fetch_result_set(sybase_ptr, buffered, store);
				if (result == NULL) {
					ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
					RETURN_FALSE;
				}
				status = Q_RESULT;
				break;
		}
		
		/* Check for left-over results */
		if (!buffered && status != Q_RESULT) {
			while ((retcode = ct_results(sybase_ptr->cmd, &restype))==CS_SUCCEED) {
				switch ((int) restype) {
					case CS_CMD_SUCCEED:
					case CS_CMD_DONE:
						break;

					case CS_CMD_FAIL:
						status = Q_FAILURE;
						break;

					case CS_COMPUTE_RESULT:
					case CS_CURSOR_RESULT:
					case CS_PARAM_RESULT:
					case CS_ROW_RESULT:
						if (status != Q_RESULT) {
							result = php_sybase_fetch_result_set(sybase_ptr, buffered, store);
							if (result == NULL) {
								ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
								sybase_ptr->dead = 1;
								RETURN_FALSE;
							}
							status = Q_RESULT;
							retcode = result->last_retcode; 
						} else {
							/* Unexpected results, cancel them. */
							ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_CURRENT);
						}
						break;
					case CS_STATUS_RESULT:
						/* Unexpected results, cancel them. */
						ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_CURRENT);
						break;

					default:
						status = Q_FAILURE;
						break;
				}
				if (status == Q_FAILURE) {
					ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
				}
				if (retcode == CS_END_RESULTS) {
					break;
				}
			}
			switch (retcode) {
				case CS_END_RESULTS:
					/* Normal. */
					break;

				case CS_FAIL:
					/* Hopefully this either cleans up the connection, or the
					 * connection ends up marked dead so it will be reopened
					 * if it is persistent.  We may want to do
					 * ct_close(CS_FORCE_CLOSE) if ct_cancel() fails; see the
					 * doc for ct_results()==CS_FAIL.
					 */
					ct_cancel(NULL, sybase_ptr->cmd, CS_CANCEL_ALL);
					/* Don't take chances with the vagaries of ct-lib.  Mark it
					 * dead ourselves.
					 */
					sybase_ptr->dead = 1;
				case CS_CANCELED:
				default:
					status = Q_FAILURE;
					break;
			}
		}

		/* Retry deadlocks up until deadlock_retry_count times */		
		if (sybase_ptr->deadlock && SybCtG(deadlock_retry_count) != -1 && ++deadlock_count > SybCtG(deadlock_retry_count)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Retried deadlock %d times [max: %ld], giving up", deadlock_count- 1, SybCtG(deadlock_retry_count));
			if (result != NULL) {
				_free_sybase_result(result);
			}
			break;
		}

		/* If query completed without deadlock, break out of the loop.
		 * Sometimes deadlock results in failures and sometimes not,
		 * it seems to depend on the server flavor.  But we want to
		 * retry all deadlocks.
		 */
		if (sybase_ptr->dead || sybase_ptr->deadlock == 0) {
			break;
		}

		/* Get rid of any results we may have fetched.  This happens:
		 * e.g., our result set may be a stored procedure status which
		 * is returned even if the stored procedure deadlocks.  As an
		 * optimization, we could try not to fetch results in known
		 * deadlock conditions, but deadlock is (should be) rare.
		 */
		if (result != NULL) {
			_free_sybase_result(result);
		}
	}

	if (status == Q_SUCCESS) {
		RETURN_TRUE;
	}

	if (status == Q_FAILURE) {
		if (result != NULL) {
			_free_sybase_result(result);
		}
		RETURN_FALSE;
	}

	/* Indicate we have data in case of buffered queries */
	id= ZEND_REGISTER_RESOURCE(return_value, result, le_result);
	sybase_ptr->active_result_index= buffered ? id : 0;
}

/* {{{ proto int sybase_query(string query [, int link_id])
   Send Sybase query */
PHP_FUNCTION(sybase_query)
{
	php_sybase_query(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int sybase_unbuffered_query(string query [, int link_id])
   Send Sybase query */
PHP_FUNCTION(sybase_unbuffered_query)
{
	php_sybase_query(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}

/* {{{ proto bool sybase_free_result(int result)
   Free result memory */
PHP_FUNCTION(sybase_free_result)
{
	zval **sybase_result_index;
	sybase_result *result;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(sybase_result_index) == IS_RESOURCE && Z_LVAL_PP(sybase_result_index) == 0) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);
	
	/* Did we fetch up until the end? */
	if (result->last_retcode != CS_END_DATA && result->last_retcode != CS_END_RESULTS) {
		/* php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Cancelling the rest of the results"); */
		ct_cancel(NULL, result->sybase_ptr->cmd, CS_CANCEL_ALL);
		php_sybase_finish_results(result);
	}
	
	zend_list_delete(Z_LVAL_PP(sybase_result_index));
	RETURN_TRUE;
}

/* }}} */

/* {{{ proto string sybase_get_last_message(void)
   Returns the last message from server (over min_message_severity) */
PHP_FUNCTION(sybase_get_last_message)
{
	RETURN_STRING(SybCtG(server_message), 1);
}
/* }}} */

/* {{{ proto int sybase_num_rows(int result)
   Get number of rows in result */
PHP_FUNCTION(sybase_num_rows)
{
	zval **sybase_result_index;
	sybase_result *result;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	Z_LVAL_P(return_value) = result->num_rows;
	Z_TYPE_P(return_value) = IS_LONG;
}

/* }}} */

/* {{{ proto int sybase_num_fields(int result)
   Get number of fields in result */
PHP_FUNCTION(sybase_num_fields)
{
	zval **sybase_result_index;
	sybase_result *result;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	Z_LVAL_P(return_value) = result->num_fields;
	Z_TYPE_P(return_value) = IS_LONG;
}

/* }}} */

/* {{{ proto array sybase_fetch_row(int result)
   Get row as enumerated array */
PHP_FUNCTION(sybase_fetch_row)
{
	zval **sybase_result_index;
	int i;
	sybase_result *result;
	zval *field_content;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	/* Unbuffered? */
	if (result->last_retcode != CS_END_DATA && result->last_retcode != CS_END_RESULTS) {
		php_sybase_fetch_result_row(result, 1);
	}
	
	/* At the end? */
	if (result->cur_row >= result->num_rows) {
		RETURN_FALSE;
	}

	array_init(return_value);
	for (i=0; i<result->num_fields; i++) {
		ALLOC_ZVAL(field_content);
		*field_content = result->data[result->store ? result->cur_row : 0][i];
		INIT_PZVAL(field_content);
		zval_copy_ctor(field_content);
		zend_hash_index_update(Z_ARRVAL_P(return_value), i, (void *) &field_content, sizeof(zval* ), NULL);
	}
	result->cur_row++;
}

/* }}} */

static void php_sybase_fetch_hash(INTERNAL_FUNCTION_PARAMETERS, int numerics)
{
	zval **sybase_result_index;
	sybase_result *result;
	int i, j;
	zval *tmp;
	char name[32];

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	/* Unbuffered ? Fetch next row */
	if (result->last_retcode != CS_END_DATA && result->last_retcode != CS_END_RESULTS) {
		php_sybase_fetch_result_row(result, 1);
	}

	/* At the end? */
	if (result->cur_row >= result->num_rows) {
		RETURN_FALSE;
	}

	array_init(return_value);
	
	j= 1;
	for (i=0; i<result->num_fields; i++) {
		ALLOC_ZVAL(tmp);
		*tmp = result->data[result->store ? result->cur_row : 0][i];
		INIT_PZVAL(tmp);
		if (PG(magic_quotes_runtime) && Z_TYPE_P(tmp) == IS_STRING) {
			Z_STRVAL_P(tmp) = php_addslashes(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp), &Z_STRLEN_P(tmp), 0 TSRMLS_CC);
		} else {
			zval_copy_ctor(tmp);
		}
		if (numerics) {
			zend_hash_index_update(Z_ARRVAL_P(return_value), i, (void *) &tmp, sizeof(zval *), NULL);
			tmp->refcount++;
		}
		
		if (zend_hash_exists(Z_ARRVAL_P(return_value), result->fields[i].name, strlen(result->fields[i].name)+1)) {
			snprintf(name, 32, "%s%d", result->fields[i].name, j);
			result->fields[i].name= estrdup(name);
			j++;
		}
		zend_hash_update(Z_ARRVAL_P(return_value), result->fields[i].name, strlen(result->fields[i].name)+1, (void *) &tmp, sizeof(zval *), NULL);
	}
	result->cur_row++;
}


/* {{{ proto object sybase_fetch_object(int result [, mixed object])
   Fetch row as object */
PHP_FUNCTION(sybase_fetch_object)
{
	zval **object= NULL;
	zval *sybase_result_index;
	zend_class_entry *ce= NULL;
	
	/* Was a second parameter given? */
	if (2 == ZEND_NUM_ARGS()) {
		if (zend_get_parameters_ex(2, &sybase_result_index, &object) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		
		switch (Z_TYPE_PP(object)) {
			case IS_OBJECT:
				ce = Z_OBJCE_PP(object);
				break;
			case IS_NULL:
				break;
			default: {
				zend_class_entry **pce = NULL;

				convert_to_string_ex(object);
				if (zend_lookup_class(Z_STRVAL_PP(object), Z_STRLEN_PP(object), &pce TSRMLS_CC) == FAILURE) {
					php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Sybase:  Class %s has not been declared", Z_STRVAL_PP(object));
				} else {
					ce = *pce;
				}
			}
		}

		/* Reset no. of arguments to 1 so that we can use INTERNAL_FUNCTION_PARAM_PASSTHRU */
		ht= 1;
	}
	
	php_sybase_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
	if (Z_TYPE_P(return_value)==IS_ARRAY) {
		object_and_properties_init(
			return_value, 
			ce ? ce : ZEND_STANDARD_CLASS_DEF_PTR, 
			Z_ARRVAL_P(return_value)
		);
	}
}
/* }}} */

/* {{{ proto array sybase_fetch_array(int result)
   Fetch row as array */
PHP_FUNCTION(sybase_fetch_array)
{
	php_sybase_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto array sybase_fetch_assoc(int result)
   Fetch row as array without numberic indices */
PHP_FUNCTION(sybase_fetch_assoc)
{
	php_sybase_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto bool sybase_data_seek(int result, int offset)
   Move internal row pointer */
PHP_FUNCTION(sybase_data_seek)
{
	zval **sybase_result_index, **offset;
	sybase_result *result;

	if (ZEND_NUM_ARGS()!=2 || zend_get_parameters_ex(2, &sybase_result_index, &offset)==FAILURE) {
		WRONG_PARAM_COUNT;
	}


	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	convert_to_long_ex(offset);

	/* Unbuffered ? */
	if (result->last_retcode != CS_END_DATA && result->last_retcode != CS_END_RESULTS && Z_LVAL_PP(offset)>=result->num_rows) {
		php_sybase_fetch_result_row(result, Z_LVAL_PP(offset)+ 1);
	}
	
	if (Z_LVAL_PP(offset)<0 || Z_LVAL_PP(offset)>=result->num_rows) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Bad row offset %ld, must be betweem 0 and %d", Z_LVAL_PP(offset), result->num_rows - 1);
		RETURN_FALSE;
	}

	result->cur_row = Z_LVAL_PP(offset);
	RETURN_TRUE;
}
/* }}} */

static char *php_sybase_get_field_name(CS_INT type)
{
	switch (type) {
		case CS_CHAR_TYPE:
		case CS_VARCHAR_TYPE:
		case CS_TEXT_TYPE:
			return "string";
			break;
		case CS_IMAGE_TYPE:
			return "image";
			break;
		case CS_BINARY_TYPE:
		case CS_VARBINARY_TYPE:
			return "blob";
			break;
		case CS_BIT_TYPE:
			return "bit";
			break;
		case CS_TINYINT_TYPE:
		case CS_SMALLINT_TYPE:
		case CS_INT_TYPE:
			return "int";
			break;
		case CS_REAL_TYPE:
		case CS_FLOAT_TYPE:
		case CS_NUMERIC_TYPE:
		case CS_DECIMAL_TYPE:
			return "real";
			break;
		case CS_MONEY_TYPE:
		case CS_MONEY4_TYPE:
			return "money";
			break;
		case CS_DATETIME_TYPE:
		case CS_DATETIME4_TYPE:
			return "datetime";
			break;
		default:
			return "unknown";
			break;
	}
}


/* {{{ proto object sybase_fetch_field(int result [, int offset])
   Get field information */
PHP_FUNCTION(sybase_fetch_field)
{
	zval **sybase_result_index, **offset;
	int field_offset;
	sybase_result *result;

	switch (ZEND_NUM_ARGS()) {
		case 1:
			if (zend_get_parameters_ex(1, &sybase_result_index) == FAILURE) {
				RETURN_FALSE;
			}
			field_offset=-1;
			break;
		case 2:
			if (zend_get_parameters_ex(2, &sybase_result_index, &offset) == FAILURE) {
				RETURN_FALSE;
			}
			convert_to_long_ex(offset);
			field_offset = Z_LVAL_PP(offset);
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	if (field_offset==-1) {
		field_offset = result->cur_field;
		result->cur_field++;
	}

	if (field_offset<0 || field_offset >= result->num_fields) {
		if (ZEND_NUM_ARGS()==2) { /* field specified explicitly */
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Bad column offset");
		}
		RETURN_FALSE;
	}

	object_init(return_value);

	add_property_string(return_value, "name", result->fields[field_offset].name, 1);
	add_property_long(return_value, "max_length", result->fields[field_offset].max_length);
	add_property_string(return_value, "column_source", result->fields[field_offset].column_source, 1);
	add_property_long(return_value, "numeric", result->fields[field_offset].numeric);
	add_property_string(return_value, "type", php_sybase_get_field_name(Z_TYPE(result->fields[field_offset])), 1);
}
/* }}} */


/* {{{ proto bool sybase_field_seek(int result, int offset)
   Set field offset */
PHP_FUNCTION(sybase_field_seek)
{
	zval **sybase_result_index, **offset;
	int field_offset;
	sybase_result *result;

	if (ZEND_NUM_ARGS() !=2 || zend_get_parameters_ex(2, &sybase_result_index, &offset) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	convert_to_long_ex(offset);
	field_offset = Z_LVAL_PP(offset);
	
	if (field_offset<0 || field_offset >= result->num_fields) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Bad column offset");
		RETURN_FALSE;
	}

	result->cur_field = field_offset;
	RETURN_TRUE;
}
/* }}} */


/* {{{ proto string sybase_result(int result, int row, mixed field)
   Get result data */
PHP_FUNCTION(sybase_result)
{
	zval **row, **field, **sybase_result_index;
	int field_offset=0;
	sybase_result *result;

	if (ZEND_NUM_ARGS() !=3 || zend_get_parameters_ex(3, &sybase_result_index, &row, &field)==FAILURE) {
		WRONG_PARAM_COUNT;
	}


	ZEND_FETCH_RESOURCE(result, sybase_result *, sybase_result_index, -1, "Sybase result", le_result);

	convert_to_long_ex(row);
	
	/* Unbuffered ? */
	if (result->last_retcode != CS_END_DATA && result->last_retcode != CS_END_RESULTS && Z_LVAL_PP(row) >= result->num_rows) {
		php_sybase_fetch_result_row(result, Z_LVAL_PP(row));
	}

	if (Z_LVAL_PP(row) < 0 || Z_LVAL_PP(row) >= result->num_rows) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Bad row offset (%ld)", Z_LVAL_PP(row));
		RETURN_FALSE;
	}

	switch(Z_TYPE_PP(field)) {
		case IS_STRING: {
			int i;

			for (i=0; i<result->num_fields; i++) {
				if (!strcasecmp(result->fields[i].name, Z_STRVAL_PP(field))) {
					field_offset = i;
					break;
				}
			}
			if (i>=result->num_fields) { /* no match found */
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  %s field not found in result", Z_STRVAL_PP(field));
				RETURN_FALSE;
			}
			break;
		}
		default:
			convert_to_long_ex(field);
			field_offset = Z_LVAL_PP(field);
			if (field_offset<0 || field_offset>=result->num_fields) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Sybase:  Bad column offset specified");
				RETURN_FALSE;
			}
			break;
	}

	*return_value = result->data[Z_LVAL_PP(row)][field_offset];
	zval_copy_ctor(return_value);
}
/* }}} */


/* {{{ proto int sybase_affected_rows([int link_id])
   Get number of affected rows in last query */
PHP_FUNCTION(sybase_affected_rows)
{
	zval **sybase_link_index;
	sybase_link *sybase_ptr;
	int id;

	switch(ZEND_NUM_ARGS()) {
		case 0:
			id = php_sybase_get_default_link(INTERNAL_FUNCTION_PARAM_PASSTHRU);
			break;
		case 1:
			if (zend_get_parameters_ex(1, &sybase_link_index) == FAILURE) {
				RETURN_FALSE;
			}
			id = -1;
			break;
		default:
			WRONG_PARAM_COUNT;
			break;
	}

	ZEND_FETCH_RESOURCE2(sybase_ptr, sybase_link *, sybase_link_index, id, "Sybase-Link", le_link, le_plink);

	Z_LVAL_P(return_value) = sybase_ptr->affected_rows;
	Z_TYPE_P(return_value) = IS_LONG;
}
/* }}} */


PHP_MINFO_FUNCTION(sybase)
{
	char buf[32];

	php_info_print_table_start();
	php_info_print_table_header(2, "Sybase_CT Support", "enabled" );
	sprintf(buf, "%ld", SybCtG(num_persistent));
	php_info_print_table_row(2, "Active Persistent Links", buf);
	sprintf(buf, "%ld", SybCtG(num_links));
	php_info_print_table_row(2, "Active Links", buf);
	sprintf(buf, "%ld", SybCtG(min_server_severity));
	php_info_print_table_row(2, "Min server severity", buf);
	sprintf(buf, "%ld", SybCtG(min_client_severity));
	php_info_print_table_row(2, "Min client severity", buf);	
	php_info_print_table_row(2, "Application Name", SybCtG(appname));
	sprintf(buf, "%ld", SybCtG(deadlock_retry_count));
	php_info_print_table_row(2, "Deadlock retry count", buf);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}


/* {{{ proto void sybase_min_client_severity(int severity)
   Sets minimum client severity */
PHP_FUNCTION(sybase_min_client_severity)
{
	zval **severity;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &severity) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(severity);
	SybCtG(min_client_severity) = Z_LVAL_PP(severity);
}
/* }}} */


/* {{{ proto void sybase_min_server_severity(int severity)
   Sets minimum server severity */
PHP_FUNCTION(sybase_min_server_severity)
{
	zval **severity;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &severity) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(severity);
	SybCtG(min_server_severity) = Z_LVAL_PP(severity);
}
/* }}} */

/* {{{ proto void sybase_deadlock_retry_count(int retry_count)
   Sets deadlock retry count */
PHP_FUNCTION(sybase_deadlock_retry_count)
{
	zval **retry_count;

	if (ZEND_NUM_ARGS() !=1 || zend_get_parameters_ex(1, &retry_count) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long_ex(retry_count);
	SybCtG(deadlock_retry_count) = Z_LVAL_PP(retry_count);
}
/* }}} */


/* {{{ proto bool sybase_set_message_handler(mixed error_func [, resource connection])
   Set the error handler, to be called when a server message is raised. 
   If error_func is NULL the handler will be deleted */
PHP_FUNCTION(sybase_set_message_handler)
{
	zval **callback, **param, **sybase_link_index= NULL;
	zval name;
	sybase_link *sybase_ptr;

	switch (ZEND_NUM_ARGS()) {
		case 1:		 /* Default message handler */
			if (zend_get_parameters_ex(1, &param) == FAILURE) {
				RETURN_FALSE;
			}
			callback = &SybCtG(callback_name);
			break;

		case 2:		 /* Connection-based message handler */
			if (zend_get_parameters_ex(2, &param, &sybase_link_index) == FAILURE) {
				RETURN_FALSE;
			}			
			ZEND_FETCH_RESOURCE2(sybase_ptr, sybase_link *, sybase_link_index, -1, "Sybase-Link", le_link, le_plink);
			callback = &sybase_ptr->callback_name;
			break;

		default:
			WRONG_PARAM_COUNT;
	}
	
	/* Clean out old callback */
	if (*callback) {
		zval_ptr_dtor(callback);
		*callback = NULL;
	}

	switch (Z_TYPE_PP(param)) {
		case IS_NULL:
			/* Return TRUE to indicate we deleted the message handler */
			RETURN_TRUE;
			break;

		case IS_ARRAY:
		case IS_STRING:
			/* Either "function", array("class", "function") or array($object, "function") */
			if (!zend_is_callable(*param, 0, &name)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argumented is expected to be a valid callback, '%R' was given", Z_TYPE(name), Z_UNIVAL(name));
				zval_dtor(&name);
				RETURN_FALSE;
			}
			zval_dtor(&name);
			break;

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argumented is expected to be either NULL, an array or string, %s given", zend_zval_type_name(*param));
			RETURN_FALSE;
	}
	
	ALLOC_ZVAL(*callback);
	**callback = **param;
	INIT_PZVAL(*callback);
	zval_copy_ctor(*callback);
	RETURN_TRUE;
}
/* }}} */


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
