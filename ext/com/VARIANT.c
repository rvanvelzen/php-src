/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Harald Radi <h.radi@nme.at>                                  |
   +----------------------------------------------------------------------+
 */


/*
 * This module maps the VARIANT datastructure into PHP so that it can be used to
 * pass values to COM and DOTNET Objects by reference and not only by value.
 *
 * harald
 */

#ifdef PHP_WIN32

#include "php.h"
#include "php_ini.h"
#include "php_typedef_VARIANT.h"
#include "conversion.h"

#include <unknwn.h> 

void php_VARIANT_call_function_handler(INTERNAL_FUNCTION_PARAMETERS, zend_property_reference *property_reference);
pval php_VARIANT_get_property_handler(zend_property_reference *property_reference);
static int do_VARIANT_propset(VARIANT *var_arg, pval *arg_property, pval *value);
void php_register_VARIANT_class();
static void php_variant_destructor(zend_rsrc_list_entry *rsrc);
void php_register_VARIANT_class();

static int le_variant;

static zend_class_entry VARIANT_class_entry;

function_entry VARIANT_functions[] = {
	{NULL, NULL, NULL}
};

static PHP_MINFO_FUNCTION(VARIANT)
{
	DISPLAY_INI_ENTRIES();
}

zend_module_entry VARIANT_module_entry = {
	"variant", VARIANT_functions, PHP_MINIT(VARIANT), PHP_MSHUTDOWN(VARIANT), NULL, NULL, PHP_MINFO(VARIANT), STANDARD_MODULE_PROPERTIES
};

PHP_MINIT_FUNCTION(VARIANT)
{
	le_variant = zend_register_list_destructors_ex(php_variant_destructor, NULL, "VARIANT", module_number);

	php_register_VARIANT_class();
	return SUCCESS;
}


PHP_MSHUTDOWN_FUNCTION(VARIANT)
{
	return SUCCESS;
}

void php_VARIANT_call_function_handler(INTERNAL_FUNCTION_PARAMETERS, zend_property_reference *property_reference)
{
	pval *object = property_reference->object;
	zend_overloaded_element *function_name = (zend_overloaded_element *) property_reference->elements_list->tail->data;
	VARIANT *pVar;

	if ((zend_llist_count(property_reference->elements_list)==1) && !strcmp(function_name->element.value.str.val, "variant"))
	{
		/* constructor */
		pval *object_handle, *data, *type;
	
		pVar = emalloc(sizeof(VARIANT));
		VariantInit(pVar);

		switch (ZEND_NUM_ARGS())
		{
			case 0:
				/* nothing to do */
				break;
			case 1:
				getParameters(ht, 1, &data);
				php_pval_to_variant(data, pVar);
				break;
			case 2:
				getParameters(ht, 2, &data, &type);
				php_pval_to_variant_ex(data, pVar, type);
				break;
			default:
				WRONG_PARAM_COUNT;
				break;
		}

		return_value->type = IS_LONG;
		return_value->value.lval = zend_list_insert(pVar, le_variant);

		if (!zend_is_true(return_value)) {
			var_reset(object);
			return;
		}

		ALLOC_ZVAL(object_handle);
		*object_handle = *return_value;
		pval_copy_constructor(object_handle);
		INIT_PZVAL(object_handle);
		zend_hash_index_update(object->value.obj.properties, 0, &object_handle, sizeof(pval *), NULL);
		pval_destructor(&function_name->element);
	}
}


pval php_VARIANT_get_property_handler(zend_property_reference *property_reference)
{
	zend_overloaded_element *overloaded_property;
	int type;
	
	pval result, **var_handle, *object = property_reference->object;
	VARIANT *var_arg;

	/* fetch the VARIANT structure */
	zend_hash_index_find(object->value.obj.properties, 0, (void **) &var_handle);
	var_arg = zend_list_find((*var_handle)->value.lval, &type);

	if (!var_arg || (type != le_variant))
		var_reset(&result);
	else
	{
		overloaded_property = (zend_overloaded_element *) property_reference->elements_list->head->data;
		switch (overloaded_property->type)
		{
			case OE_IS_ARRAY:
				var_reset(&result);
				break;

			case OE_IS_OBJECT:
				if(!strcmp(overloaded_property->element.value.str.val, "value"))
					php_variant_to_pval(var_arg, &result, 0);
				else
				{
					var_reset(&result);
					php_error(E_WARNING, "Unknown member.");
				}
				break;
			case OE_IS_METHOD:
				var_reset(&result);
				php_error(E_WARNING, "Unknown method.");
				break;
		
			pval_destructor(&overloaded_property->element);
		}
	}
	
	return result;
}

int php_VARIANT_set_property_handler(zend_property_reference *property_reference, pval *value)
{
	zend_overloaded_element *overloaded_property;
	int type;
	
	pval **var_handle, *object = property_reference->object;
	VARIANT *var_arg;

	/* fetch the VARIANT structure */
	zend_hash_index_find(object->value.obj.properties, 0, (void **) &var_handle);
	var_arg = zend_list_find((*var_handle)->value.lval, &type);

	if (!var_arg || (type != le_variant))
		return FAILURE;
	
	overloaded_property = (zend_overloaded_element *) property_reference->elements_list->head->data;
	do_VARIANT_propset(var_arg, &overloaded_property->element, value);
	pval_destructor(&overloaded_property->element);
	return SUCCESS;
}

static int do_VARIANT_propset(VARIANT *var_arg, pval *arg_property, pval *value)
{
	pval type;

	type.type = IS_STRING;
	
	if(!strcmp(arg_property->value.str.val, "bVal"))
	{
		type.value.str.val = "VT_UI1";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "iVal"))
	{
		type.value.str.val = "VT_I2";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "lVal"))
	{
		type.value.str.val = "VT_I4";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "fltVal"))
	{
		type.value.str.val = "VT_R4";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "dblVal"))
	{
		type.value.str.val = "VT_R8";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "boolVal"))
	{
		type.value.str.val = "VT_BOOL";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "scode"))
	{
		type.value.str.val = "VT_ERROR";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "cyVal"))
	{
		type.value.str.val = "VT_CY";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "date"))
	{
		type.value.str.val = "VT_DATE";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "bstrVal"))
	{
		type.value.str.val = "VT_BSTR";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pdecVal"))
	{
		type.value.str.val = "VT_DECIMAL|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "punkVal"))
	{
		type.value.str.val = "VT_UNKNOWN";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pdispVal"))
	{
		type.value.str.val = "VT_DISPATCH";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "parray"))
	{
		type.value.str.val = "VT_ARRAY";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pbVal"))
	{
		type.value.str.val = "VT_UI1|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "piVal"))
	{
		type.value.str.val = "VT_I2|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "plVal"))
	{
		type.value.str.val = "VT_I4|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pfltVal"))
	{
		type.value.str.val = "VT_R4|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pdblVal"))
	{
		type.value.str.val = "VT_R8|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pboolVal"))
	{
		type.value.str.val = "VT_BOOL|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pscode"))
	{
		type.value.str.val = "VT_ERROR|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pcyVal"))
	{
		type.value.str.val = "VT_CY|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pdate"))
	{
		type.value.str.val = "VT_DATE|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pbstrVal"))
	{
		type.value.str.val = "VT_BSTR|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "ppunkVal"))
	{
		type.value.str.val = "VT_UNKNOWN|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "ppdispVal"))
	{
		type.value.str.val = "VT_DISPATCH|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pparray"))
	{
		type.value.str.val = "VT_ARRAY|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pvarVal"))
	{
		type.value.str.val = "VT_VARIANT|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "byref"))
	{
		type.value.str.val = "VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "cVal"))
	{
		type.value.str.val = "VT_I1";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "uiVal"))
	{
		type.value.str.val = "VT_UI2";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "ulVal"))
	{
		type.value.str.val = "VT_UI4";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "intVal"))
	{
		type.value.str.val = "VT_INT";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "uintVal"))
	{
		type.value.str.val = "VT_UINT|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pcVal"))
	{
		type.value.str.val = "VT_I1|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "puiVal"))
	{
		type.value.str.val = "VT_UI2|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pulVal"))
	{
		type.value.str.val = "VT_UI4|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "pintVal"))
	{
		type.value.str.val = "VT_INT|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else if(!strcmp(arg_property->value.str.val, "puintVal"))
	{
		type.value.str.val = "VT_UINT|VT_BYREF";
		php_pval_to_variant_ex(value, var_arg, &type);
	}
	else
	{
		php_error(E_WARNING, "Unknown member.");
		return FAILURE;
	}
	return SUCCESS;
}

static void php_variant_destructor(zend_rsrc_list_entry *rsrc)
{
	efree(rsrc);
}

void php_register_VARIANT_class()
{
	INIT_OVERLOADED_CLASS_ENTRY(VARIANT_class_entry, "VARIANT", NULL,
								php_VARIANT_call_function_handler,
								php_VARIANT_get_property_handler,
								php_VARIANT_set_property_handler);

	zend_register_internal_class(&VARIANT_class_entry);
}

#endif /* PHP_WIN32 */
