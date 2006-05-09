/*
   +----------------------------------------------------------------------+
   | PHP Version 6                                                        |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Andrei Zmievski <andrei@php.net>                            |
   +----------------------------------------------------------------------+
 */

/* $Id$ */ 

#include "php_unicode.h"

typedef UBool (*prop_check_func_t)(UChar32 ch);

typedef struct {
	zval	  			  **args[4];
	void	 			 ***thread_ctx;
	zend_fcall_info 		fci;
	zend_fcall_info_cache	fci_cache;
} char_enum_context_t;

static void check_property_impl(INTERNAL_FUNCTION_PARAMETERS, prop_check_func_t checker)
{
	UChar	   *str;
	int			str_len;
	zend_bool	result = 1;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}

	while (offset < str_len && result) {
		U16_NEXT(str, offset, str_len, ch);
		result = checker(ch);
	}

	RETURN_BOOL(result);
}

/* {{{ C/POSIX migration functions */

PHP_FUNCTION(char_is_lower)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_islower);
}

PHP_FUNCTION(char_is_upper)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isupper);
}

PHP_FUNCTION(char_is_digit)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isdigit);
}

PHP_FUNCTION(char_is_alpha)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isalpha);
}

PHP_FUNCTION(char_is_alnum)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isalnum);
}

PHP_FUNCTION(char_is_xdigit)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isxdigit);
}

PHP_FUNCTION(char_is_punct)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_ispunct);
}

PHP_FUNCTION(char_is_graph)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isgraph);
}

PHP_FUNCTION(char_is_blank)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isblank);
}

PHP_FUNCTION(char_is_space)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isspace);
}

PHP_FUNCTION(char_is_cntrl)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_iscntrl);
}

PHP_FUNCTION(char_is_print)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isprint);
}

/* }}} */

/* {{{ Additional binary property functions */

PHP_FUNCTION(char_is_defined)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isdefined);
}

PHP_FUNCTION(char_is_id_start)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isIDStart);
}

PHP_FUNCTION(char_is_id_part)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isIDPart);
}

PHP_FUNCTION(char_is_id_ignorable)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isIDIgnorable);
}

PHP_FUNCTION(char_is_iso_control)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isISOControl);
}

PHP_FUNCTION(char_is_mirrored)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isMirrored);
}

PHP_FUNCTION(char_is_base)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isbase);
}

PHP_FUNCTION(char_is_whitespace)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isUWhiteSpace);
}

PHP_FUNCTION(char_is_alphabetic)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isUAlphabetic);
}

PHP_FUNCTION(char_is_uppercase)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isUUppercase);
}

PHP_FUNCTION(char_is_lowercase)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_isULowercase);
}

PHP_FUNCTION(char_is_titlecase)
{
	check_property_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, u_istitle);
}


/* }}} */

/* {{{ Single character properties */

PHP_FUNCTION(char_get_numeric_value)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_DOUBLE(u_getNumericValue(ch));
}

PHP_FUNCTION(char_get_combining_class)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_LONG((long)u_getCombiningClass(ch));
}

PHP_FUNCTION(char_get_digit_value)
{
	UChar	   *str;
	int			str_len;
	int			radix = 0;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u|l", &str, &str_len, &radix) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	if (ZEND_NUM_ARGS() > 1) {
		if (radix < 2 || radix > 36) {
			php_error(E_WARNING, "Radix has to be in 2-36 range");
			return;
		}
		RETURN_LONG(u_digit(ch, radix));
	} else {
		RETURN_LONG(u_charDigitValue(ch));
	}
}

PHP_FUNCTION(char_get_mirrored)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_UCHAR32(u_charMirror(ch));
}

PHP_FUNCTION(char_get_direction)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_LONG((long)u_charDirection(ch));
}

PHP_FUNCTION(char_get_age)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;
	UVersionInfo version;
	char        buf[18] = { 0, };

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	u_charAge(ch, version);
	u_versionToString(version, buf);

	RETURN_ASCII_STRING(buf, ZSTR_DUPLICATE);
}

PHP_FUNCTION(char_get_type)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_LONG(u_charType(ch));
}

PHP_FUNCTION(char_is_valid)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u", &str, &str_len) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}
	U16_NEXT(str, offset, str_len, ch);

	RETURN_BOOL(U_IS_UNICODE_CHAR(ch));
}

PHP_FUNCTION(char_from_digit)
{
	int			digit;
	int			radix = 10;
	UChar32		ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &digit, &radix) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() > 1) {
		if (radix < 2 || radix > 36) {
			php_error(E_WARNING, "Radix has to be in 2-36 range");
			return;
		}
	}
	ch = u_forDigit(digit, radix);
	
	if (ch == (UChar32)0) {
		RETURN_FALSE;
	}

	RETURN_UCHAR32(ch);
}

PHP_FUNCTION(char_from_name)
{
	void	   *name;
	int			name_len;
	zend_uchar	name_type;
	UChar32		ch;
	UCharNameChoice choice = U_UNICODE_CHAR_NAME;
	zend_bool	extended = FALSE;
	char	   *buf;
	UErrorCode	status = U_ZERO_ERROR;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "t|b", &name, &name_len, &name_type, &extended) == FAILURE) {
		return;
	}

	if (extended) {
		choice = U_EXTENDED_CHAR_NAME;
	}

	if (name_type == IS_UNICODE) {
		buf = zend_unicode_to_ascii(name, name_len TSRMLS_CC);
		if (buf == NULL) {
			php_error(E_WARNING, "Character name has to consist only of ASCII characters");
			RETURN_FALSE;
		}
	} else {
		buf = (char *) name;
	}

	ch = u_charFromName(choice, buf, &status);

	if (name_type == IS_UNICODE) {
		efree(buf);
	}

	if (U_SUCCESS(status)) {
		RETURN_UCHAR32(ch);
	} else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(char_get_name)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;
	zend_bool	extended = FALSE;
	UCharNameChoice choice = U_UNICODE_CHAR_NAME;
	char	   *buf;
	int			buf_len = 128;
	UErrorCode  status = U_ZERO_ERROR;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "u|b", &str, &str_len, &extended) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}

	if (extended) {
		choice = U_EXTENDED_CHAR_NAME;
	}

	U16_NEXT(str, offset, str_len, ch);

	buf = emalloc(buf_len);
	buf_len = u_charName(ch, choice, buf, buf_len, &status);
	if (buf_len == 0) {
		efree(buf);
		RETURN_FALSE;
	} else if (status == U_BUFFER_OVERFLOW_ERROR) {
		status = U_ZERO_ERROR;
		buf = erealloc(buf, buf_len+1);
		buf_len = u_charName(ch, choice, buf, buf_len+1, &status);
	}

	RETURN_ASCII_STRINGL(buf, buf_len, ZSTR_AUTOFREE);
}

/* }}} */

/* {{{ Other property functions */

PHP_FUNCTION(char_has_binary_property)
{
	UChar 		*str = NULL;
	int    		 str_len;
	long   		 prop;
	UProperty	 uprop; 
	int			 offset = 0;
	zend_bool	 result = 1;
	UChar32		 ch;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ul", &str, &str_len, &prop) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}

	uprop = (UProperty)prop;

	while (offset < str_len && result) {
		U16_NEXT(str, offset, str_len, ch);
		result = u_hasBinaryProperty(ch, uprop);
	}

	RETURN_BOOL(result);
}

PHP_FUNCTION(char_get_property_value)
{
	UChar	   *str;
	int			str_len;
	int			offset = 0;
	UChar32		ch;
	long		prop;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ul", &str, &str_len, &prop) == FAILURE) {
		return;
	}

	if (str_len == 0) {
		RETURN_FALSE;
	}

	U16_NEXT(str, offset, str_len, ch);

	if (prop >= UCHAR_BINARY_START && prop < UCHAR_BINARY_LIMIT) {
		RETURN_BOOL((zend_bool)u_getIntPropertyValue(ch, (UProperty)prop));
	} else {
		RETURN_LONG(u_getIntPropertyValue(ch, (UProperty)prop));
	}
}

PHP_FUNCTION(char_get_property_min_value)
{
	long prop;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &prop) == FAILURE) {
		return;
	}
	
	RETURN_LONG(u_getIntPropertyMinValue((UProperty)prop));
}

PHP_FUNCTION(char_get_property_max_value)
{
	long prop;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &prop) == FAILURE) {
		return;
	}
	
	RETURN_LONG(u_getIntPropertyMaxValue((UProperty)prop));
}

PHP_FUNCTION(char_get_property_name)
{
	long 		 prop;
	long		 name_choice = U_LONG_PROPERTY_NAME;
	const char	*name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &prop, &name_choice) == FAILURE) {
		return;
	}

	if (name_choice < 0) {
		name_choice = U_LONG_PROPERTY_NAME;
	}

	name = u_getPropertyName((UProperty) prop, (UPropertyNameChoice) name_choice);
	if (name) {
		RETURN_ASCII_STRING((char *)name, ZSTR_DUPLICATE);
	} else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(char_get_property_from_name)
{
	void	   *name;
	int			name_len;
	zend_uchar	name_type;
	char	   *buf;
	UProperty	prop;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "t", &name, &name_len, &name_type) == FAILURE) {
		return;
	}

	if (name_type == IS_UNICODE) {
		buf = zend_unicode_to_ascii(name, name_len TSRMLS_CC);
		if (buf == NULL) {
			php_error(E_WARNING, "Property name has to consist only of ASCII characters");
			RETURN_FALSE;
		}
	} else {
		buf = (char *) name;
	}

	prop = u_getPropertyEnum(buf);
	if (name_type == IS_UNICODE) {
		efree(buf);
	}

	RETURN_LONG(prop);
}

PHP_FUNCTION(char_get_property_value_name)
{
	long 		 prop;
	long		 value;
	long		 name_choice = U_LONG_PROPERTY_NAME;
	const char	*name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll|l", &prop, &value, &name_choice) == FAILURE) {
		return;
	}

	if (name_choice < 0) {
		name_choice = U_LONG_PROPERTY_NAME;
	}

	name = u_getPropertyValueName((UProperty) prop, (int32_t) value, (UPropertyNameChoice) name_choice);
	if (name) {
		RETURN_ASCII_STRING((char *)name, ZSTR_DUPLICATE);
	} else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(char_get_property_value_from_name)
{
	long		prop;
	void	   *name;
	int			name_len;
	zend_uchar	name_type;
	char	   *buf;
	long		value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lt", &prop, &name, &name_len, &name_type) == FAILURE) {
		return;
	}

	if (name_type == IS_UNICODE) {
		buf = zend_unicode_to_ascii(name, name_len TSRMLS_CC);
		if (buf == NULL) {
			php_error(E_WARNING, "Property value name has to consist only of ASCII characters");
			RETURN_FALSE;
		}
	} else {
		buf = (char *) name;
	}

	value = u_getPropertyValueEnum((UProperty)prop, buf);
	if (name_type == IS_UNICODE) {
		efree(buf);
	}

	RETURN_LONG(value);
}

/* }}} */

/* {{{ Enumerator functions */

static UBool php_enum_char_names(const void *context,
								 UChar32 code,
								 UCharNameChoice nameChoice,
								 const char *name,
								 int32_t length)
{
    char_enum_context_t	   *ctx = (char_enum_context_t *)context;
    zval				   *retval_ptr = NULL;
    int						status;
    UBool					result = FALSE;
    TSRMLS_FETCH_FROM_CTX(ctx->thread_ctx);

    convert_to_long_ex(ctx->args[0]);
    convert_to_unicode_ex(ctx->args[1]);
    convert_to_boolean_ex(ctx->args[2]);

    ZVAL_LONG(*ctx->args[0], code);
	if (Z_USTRVAL_PP(ctx->args[1])) {
		efree(Z_USTRVAL_PP(ctx->args[1]));
	}
    ZVAL_ASCII_STRINGL(*ctx->args[1], (char *)name, length, ZSTR_DUPLICATE);
    ZVAL_BOOL(*ctx->args[2], nameChoice == U_EXTENDED_CHAR_NAME);

    ctx->fci.retval_ptr_ptr = &retval_ptr;

    status = zend_call_function(&ctx->fci, &ctx->fci_cache TSRMLS_CC);

    if (status == SUCCESS && retval_ptr && !EG(exception)) {
        convert_to_boolean(retval_ptr);
        result = (UBool)Z_BVAL_P(retval_ptr);
    } else {
        if (!EG(exception)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Enumeration callback encountered an error");
        }
        result = FALSE;
    }
    if (retval_ptr) {
        zval_ptr_dtor(&retval_ptr);
    }
    return result;
}

static UBool php_enum_char_type_range(const void *context,
									  UChar32 start,
									  UChar32 limit,
									  UCharCategory type)
{
    char_enum_context_t	   *ctx = (char_enum_context_t *)context;
    zval				   *retval_ptr = NULL;
    int						status;
    UBool					result = FALSE;
    TSRMLS_FETCH_FROM_CTX(ctx->thread_ctx);

    convert_to_long_ex(ctx->args[0]);
    convert_to_long_ex(ctx->args[1]);
    convert_to_long_ex(ctx->args[2]);

    ZVAL_LONG(*ctx->args[0], start);
    ZVAL_LONG(*ctx->args[1], limit);
    ZVAL_LONG(*ctx->args[2], type);

    ctx->fci.retval_ptr_ptr = &retval_ptr;

    status = zend_call_function(&ctx->fci, &ctx->fci_cache TSRMLS_CC);

    if (status == SUCCESS && retval_ptr && !EG(exception)) {
        convert_to_boolean(retval_ptr);
        result = (UBool)Z_BVAL_P(retval_ptr);
    } else {
        if (!EG(exception)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Enumeration callback encountered an error");
        }
        result = FALSE;
    }
    if (retval_ptr) {
        zval_ptr_dtor(&retval_ptr);
    }
    return result;
}

PHP_FUNCTION(char_enum_names)
{
    zval			   *callback;
    long				start, limit;
	zend_bool			extended = FALSE;
    zval			   *zcode, *zname, *zextended;
    char_enum_context_t ectx;
	UCharNameChoice		choice = U_UNICODE_CHAR_NAME;
    UErrorCode 			status = U_ZERO_ERROR;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "zll|l", &callback, &start, &limit, &extended)) {
        return;               
    }   
    
	if (!zend_is_callable(callback, 0, NULL)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid enumeration callback");
		return;
	}

	if (extended) {
		choice = U_EXTENDED_CHAR_NAME;
	}  

    /* Do all the heavy lifing once, instead of in the callback */
    MAKE_STD_ZVAL(zcode);
    MAKE_STD_ZVAL(zextended);
    MAKE_STD_ZVAL(zname);

    ZVAL_LONG(zcode, 0);
    ZVAL_BOOL(zextended, 0);
    Z_TYPE_P(zname) = IS_UNICODE;
	Z_USTRVAL_P(zname) = NULL;

    memset(&ectx, 0, sizeof(char_enum_context_t));
    ectx.fci.size = sizeof(ectx.fci);
    ectx.fci.function_table = EG(function_table);
    ectx.fci.function_name = callback;
    ectx.fci.no_separation = 1;
    ectx.fci_cache = empty_fcall_info_cache;
    ectx.args[0] = &zcode;
    ectx.args[1] = &zname;
    ectx.args[2] = &zextended;
    ectx.fci.param_count = 3;
    ectx.fci.params = ectx.args;
    TSRMLS_SET_CTX(ectx.thread_ctx);

	u_enumCharNames(start, limit, (UEnumCharNamesFn *)php_enum_char_names,
					(void *)&ectx, choice, &status);

    zval_ptr_dtor(&zcode);
    zval_ptr_dtor(&zextended);
    zval_ptr_dtor(&zname);

	if (U_SUCCESS(status)) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(char_enum_types)
{
    zval			   *callback;
    zval			   *zstart, *zlimit, *ztype;
    char_enum_context_t ectx;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &callback)) {
        return;               
    }   
    
	if (!zend_is_callable(callback, 0, NULL)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "invalid enumeration callback");
		return;
	}

    /* Do all the heavy lifing once, instead of in the callback */
    MAKE_STD_ZVAL(zstart);
    MAKE_STD_ZVAL(zlimit);
    MAKE_STD_ZVAL(ztype);

    ZVAL_LONG(zstart, 0);
	ZVAL_LONG(zlimit, 0);
	ZVAL_LONG(ztype,  0);

    memset(&ectx, 0, sizeof(char_enum_context_t));
    ectx.fci.size = sizeof(ectx.fci);
    ectx.fci.function_table = EG(function_table);
    ectx.fci.function_name = callback;
    ectx.fci.no_separation = 1;
    ectx.fci_cache = empty_fcall_info_cache;
    ectx.args[0] = &zstart;
    ectx.args[1] = &zlimit;
    ectx.args[2] = &ztype;
    ectx.fci.param_count = 3;
    ectx.fci.params = ectx.args;
    TSRMLS_SET_CTX(ectx.thread_ctx);

	u_enumCharTypes((UCharEnumTypeRange *)php_enum_char_type_range, (void *)&ectx);

    zval_ptr_dtor(&zstart);
    zval_ptr_dtor(&zlimit);
    zval_ptr_dtor(&ztype);

	RETURN_TRUE;
}

/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
