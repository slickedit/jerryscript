/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-builtins.h"
#include "ecma-builtin-object.h"
#include "ecma-exceptions.h"
#include "ecma-function-object.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-objects-general.h"
#include "ecma-proxy-object.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmaproxyobject ECMA Proxy object related routines
 * @{
 */

#if ENABLED (JERRY_ES2015_BUILTIN_PROXY)
/**
 * Check whether the argument satifies the requrements of [[ProxyTarget]] or [[ProxyHandler]]
 *
 * See also:
 *          ES2015 9.5.15.1-2
 *          ES2015 9.5.15.3-4
 *
 * @return true - if the arguments can be a valid [[ProxyTarget]] or [[ProxyHandler]]
 *         false - otherwise
 */
static bool
ecma_proxy_validate (ecma_value_t argument) /**< argument to validate */
{
  if (ecma_is_value_object (argument))
  {
    ecma_object_t *obj_p = ecma_get_object_from_value (argument);

    return (!ECMA_OBJECT_IS_PROXY (obj_p)
            || !ecma_is_value_null (((ecma_proxy_object_t *) obj_p)->handler));
  }

  return false;
} /* ecma_proxy_validate */

/**
 * ProxyCreate operation for create a new proxy object
 *
 * See also:
 *         ES2015 9.5.15
 *
 * @return created Proxy object as an ecma-value - if success
 *         raised error - otherwise
 */
ecma_object_t *
ecma_proxy_create (ecma_value_t target, /**< proxy target */
                   ecma_value_t handler) /**< proxy handler */
{
  /* 1 - 4. */
  if (!ecma_proxy_validate (target) || !ecma_proxy_validate (handler))
  {
    ecma_raise_type_error (ECMA_ERR_MSG ("Cannot create proxy with a non-object target or handler"));
    return NULL;
  }

  /* 5 - 6. */
  ecma_object_t *obj_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE),
                                             sizeof (ecma_proxy_object_t),
                                             ECMA_OBJECT_TYPE_PROXY);

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 8. */
  proxy_obj_p->target = target;
  /* 9. */
  proxy_obj_p->handler = handler;

  /* 10. */
  return obj_p;
} /* ecma_proxy_create */

/**
 * Definition of Proxy Revocation Function
 *
 * See also:
 *         ES2015 26.2.2.1.1
 *
 * @return ECMA_VALUE_UNDEFINED
 */
ecma_value_t
ecma_proxy_revoke_cb (const ecma_value_t function_obj, /**< the function itself */
                      const ecma_value_t this_val, /**< this_arg of the function */
                      const ecma_value_t args_p[], /**< argument list */
                      const ecma_length_t args_count) /**< argument number */
{
  JERRY_UNUSED_3 (this_val, args_p, args_count);

  ecma_object_t *func_obj_p = ecma_get_object_from_value (function_obj);

  /* 1. */
  ecma_revocable_proxy_object_t *rev_proxy_p = (ecma_revocable_proxy_object_t *) func_obj_p;

  /* 2. */
  if (ecma_is_value_null (rev_proxy_p->proxy))
  {
    return ECMA_VALUE_UNDEFINED;
  }

  /* 4. */
  ecma_proxy_object_t *proxy_p = (ecma_proxy_object_t *) ecma_get_object_from_value (rev_proxy_p->proxy);
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY ((ecma_object_t *) proxy_p));

  /* 3. */
  rev_proxy_p->proxy = ECMA_VALUE_NULL;

  /* 5. */
  proxy_p->target = ECMA_VALUE_NULL;

  /* 6. */
  proxy_p->handler = ECMA_VALUE_NULL;

  /* 7. */
  return ECMA_VALUE_UNDEFINED;
} /* ecma_proxy_revoke_cb */

/**
 * Proxy.revocable operation for create a new revocable proxy object
 *
 * See also:
 *         ES2015 26.2.2.1
 *
 * @return NULL - if the operation fails
 *         pointer to the newly created revocable proxy object - otherwise
 */
ecma_object_t *
ecma_proxy_create_revocable (ecma_value_t target, /**< target argument */
                             ecma_value_t handler) /**< handler argument */
{
  /* 1. */
  ecma_object_t *proxy_p = ecma_proxy_create (target, handler);

  /* 2. */
  if (proxy_p == NULL)
  {
    return proxy_p;
  }

  ecma_value_t proxy_value = ecma_make_object_value (proxy_p);

  /* 3. */
  ecma_object_t *func_obj_p;
  func_obj_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_FUNCTION_PROTOTYPE),
                                   sizeof (ecma_revocable_proxy_object_t),
                                   ECMA_OBJECT_TYPE_EXTERNAL_FUNCTION);

  ecma_revocable_proxy_object_t *rev_proxy_p = (ecma_revocable_proxy_object_t *) func_obj_p;
  rev_proxy_p->header.u.external_handler_cb = ecma_proxy_revoke_cb;
  /* 4. */
  rev_proxy_p->proxy = proxy_value;

  ecma_property_value_t *prop_value_p;
  ecma_value_t revoker = ecma_make_object_value (func_obj_p);

  /* 5. */
  ecma_object_t *obj_p = ecma_create_object (ecma_builtin_get (ECMA_BUILTIN_ID_OBJECT_PROTOTYPE),
                                             0,
                                             ECMA_OBJECT_TYPE_GENERAL);

  /* 6. */
  prop_value_p = ecma_create_named_data_property (obj_p,
                                                  ecma_get_magic_string (LIT_MAGIC_STRING_PROXY),
                                                  ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                  NULL);
  prop_value_p->value = proxy_value;

  /* 7. */
  prop_value_p = ecma_create_named_data_property (obj_p,
                                                  ecma_get_magic_string (LIT_MAGIC_STRING_REVOKE),
                                                  ECMA_PROPERTY_CONFIGURABLE_ENUMERABLE_WRITABLE,
                                                  NULL);
  prop_value_p->value = revoker;

  ecma_deref_object (proxy_p);
  ecma_deref_object (func_obj_p);

  /* 8. */
  return obj_p;
} /* ecma_proxy_create_revocable */

/**
 * Internal find property operation for Proxy object
 *
 * Note: Returned value must be freed with ecma_free_value.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_NOT_FOUND - if the property is not found
 *         value of the property - otherwise
 */
ecma_value_t
ecma_proxy_object_find (ecma_object_t *obj_p, /**< proxy object */
                        ecma_string_t *prop_name_p) /**< property name */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_value_t has_result = ecma_proxy_object_has (obj_p, prop_name_p);

  if (ECMA_IS_VALUE_ERROR (has_result))
  {
    return has_result;
  }

  if (ecma_is_value_false (has_result))
  {
    return ECMA_VALUE_NOT_FOUND;
  }

  return ecma_proxy_object_get (obj_p, prop_name_p, ecma_make_object_value (obj_p));
} /* ecma_proxy_object_find */

/**
 * Convert the result of the ecma_proxy_object_get_prototype_of to compressed pointer
 *
 * Note: if `proto` is non-null, the reference from the object is released
 *
 * @return compressed pointer to the `proto` value
 */
jmem_cpointer_t
ecma_proxy_object_prototype_to_cp (ecma_value_t proto) /**< ECMA_VALUE_NULL or object */
{
  JERRY_ASSERT (ecma_is_value_null (proto) || ecma_is_value_object (proto));

  if (ecma_is_value_null (proto))
  {
    return JMEM_CP_NULL;
  }

  jmem_cpointer_t proto_cp;
  ecma_object_t *proto_obj_p = ecma_get_object_from_value (proto);
  ECMA_SET_POINTER (proto_cp, proto_obj_p);
  ecma_deref_object (proto_obj_p);

  return proto_cp;
} /* ecma_proxy_object_prototype_to_cp */

/**
 * Helper method for validate the proxy object
 *
 * @return proxy trap - if the validation is successful
 *         ECMA_VALUE_ERROR - otherwise
 */
static ecma_value_t
ecma_validate_proxy_object (ecma_value_t handler, /**< proxy handler */
                            lit_magic_string_id_t magic_id) /**< routine magic id */
{
  if (ecma_is_value_null (handler))
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Handler can not be null"));
  }

  JERRY_ASSERT (ecma_is_value_object (handler));

  return ecma_op_get_method_by_magic_id (handler, magic_id);
} /* ecma_validate_proxy_object */

/* Interal operations */

/**
 * The Proxy object [[GetPrototypeOf]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.1
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_NULL or valid object (prototype) otherwise
 */
ecma_value_t
ecma_proxy_object_get_prototype_of (ecma_object_t *obj_p) /**< proxy object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 1. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 2-5. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_GET_PROTOTYPE_OF_UL);

  /* 6. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 7. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_builtin_object_object_get_prototype_of (target_obj_p);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);

  /* 8. */
  ecma_value_t handler_proto = ecma_op_function_call (func_obj_p, handler, &target, 1);

  ecma_deref_object (func_obj_p);

  /* 9. */
  if (ECMA_IS_VALUE_ERROR (handler_proto))
  {
    return handler_proto;
  }

  /* 10. */
  if (!ecma_is_value_object (handler_proto) && !ecma_is_value_null (handler_proto))
  {
    ecma_free_value (handler_proto);

    return ecma_raise_type_error (ECMA_ERR_MSG ("Trap returned neither object nor null."));
  }

  /* 11. */
  ecma_value_t extensible_target = ecma_builtin_object_object_is_extensible (target_obj_p);

  /* 12. */
  if (ECMA_IS_VALUE_ERROR (extensible_target))
  {
    ecma_free_value (handler_proto);

    return extensible_target;
  }

  /* 13. */
  if (ecma_is_value_true (extensible_target))
  {
    return handler_proto;
  }

  /* 14. */
  ecma_value_t target_proto = ecma_builtin_object_object_get_prototype_of (target_obj_p);

  /* 15. */
  if (ECMA_IS_VALUE_ERROR (target_proto))
  {
    return target_proto;
  }

  ecma_value_t ret_value = handler_proto;

  /* 16. */
  if (handler_proto != target_proto)
  {
    ecma_free_value (handler_proto);

    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Proxy target is non-extensible, but the trap did not "
                                                     "return its actual prototype."));
  }

  ecma_free_value (target_proto);

  /* 17. */
  return ret_value;
} /* ecma_proxy_object_get_prototype_of */

/**
 * The Proxy object [[SetPrototypeOf]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.2
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE/FALSE} - depends on whether the new prototype can be set for the given object
 */
ecma_value_t
ecma_proxy_object_set_prototype_of (ecma_object_t *obj_p, /**< proxy object */
                                    ecma_value_t proto) /**< new prototype object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  /* 1. */
  JERRY_ASSERT (ecma_is_value_object (proto) || ecma_is_value_null (proto));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 2. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 3-6. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_SET_PROTOTYPE_OF_UL);

  /* 7.*/
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 8. */
  if (ecma_is_value_undefined (trap))
  {
    if (ECMA_OBJECT_IS_PROXY (target_obj_p))
    {
      return ecma_proxy_object_set_prototype_of (target_obj_p, proto);
    }

    return ecma_op_ordinary_object_set_prototype_of (target_obj_p, proto);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t args[] = { target, proto };

  /* 9. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, args, 2);

  ecma_deref_object (func_obj_p);

  /* 10. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  /* 11. */
  ecma_value_t extensible_target = ecma_builtin_object_object_is_extensible (target_obj_p);

  /* 12. */
  if (ECMA_IS_VALUE_ERROR (extensible_target))
  {
    return extensible_target;
  }

  /* 13. */
  if (ecma_is_value_true (extensible_target))
  {
    return ecma_make_boolean_value (boolean_trap_result);
  }

  /* 14. */
  ecma_value_t target_proto = ecma_builtin_object_object_get_prototype_of (target_obj_p);

  /* 15. */
  if (ECMA_IS_VALUE_ERROR (target_proto))
  {
    return target_proto;
  }

  ecma_value_t ret_value = ecma_make_boolean_value (boolean_trap_result);

  /* 16. */
  if (boolean_trap_result && (target_proto != proto))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Target object is non-extensible and trap "
                                                     "returned different prototype."));
  }

  ecma_free_value (target_proto);

  /* 17. */
  return ret_value;
} /* ecma_proxy_object_set_prototype_of */

/**
 * The Proxy object [[isExtensible]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.3
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE/FALSE} - depends on whether the object is extensible
 */
ecma_value_t
ecma_proxy_object_is_extensible (ecma_object_t *obj_p) /**< proxy object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 1. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 2-5. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_IS_EXTENSIBLE);

  /* 6. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 7. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_builtin_object_object_is_extensible (target_obj_p);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);

  /* 8. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, &target, 1);

  ecma_deref_object (func_obj_p);

  /* 9. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  bool target_result;

  /* 10. */
  if (ECMA_OBJECT_IS_PROXY (target_obj_p))
  {
    ecma_value_t proxy_is_ext = ecma_proxy_object_is_extensible (target_obj_p);

    if (ECMA_IS_VALUE_ERROR (proxy_is_ext))
    {
      return proxy_is_ext;
    }

    target_result = ecma_is_value_true (proxy_is_ext);
  }
  else
  {
    target_result = ecma_op_ordinary_object_is_extensible (target_obj_p);
  }

  /* 12. */
  if (boolean_trap_result != target_result)
  {
    return ecma_raise_type_error (ECMA_ERR_MSG ("Trap result does not reflect extensibility of proxy target"));
  }

  return ecma_make_boolean_value (boolean_trap_result);
} /* ecma_proxy_object_is_extensible */

/**
 * The Proxy object [[PreventExtensions]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.4
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE/FALSE} - depends on whether the object can be set as inextensible
 */
ecma_value_t
ecma_proxy_object_prevent_extensions (ecma_object_t *obj_p) /**< proxy object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 1. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 2-5. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_PREVENT_EXTENSIONS_UL);

  /* 6. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 7. */
  if (ecma_is_value_undefined (trap))
  {
    ecma_value_t ret_value = ecma_builtin_object_object_prevent_extensions (target_obj_p);

    if (ECMA_IS_VALUE_ERROR (ret_value))
    {
      return ret_value;
    }

    ecma_deref_object (target_obj_p);

    return ECMA_VALUE_TRUE;
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);

  /* 8. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, &target, 1);

  ecma_deref_object (func_obj_p);

  /* 9. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  /* 10. */
  if (boolean_trap_result)
  {
    ecma_value_t target_is_ext = ecma_builtin_object_object_is_extensible (target_obj_p);

    if (ECMA_IS_VALUE_ERROR (target_is_ext))
    {
      return target_is_ext;
    }

    if (ecma_is_value_true (target_is_ext))
    {
      return ecma_raise_type_error (ECMA_ERR_MSG ("Trap result does not reflect inextensibility of proxy target"));
    }
  }

  /* 11. */
  return ecma_make_boolean_value (boolean_trap_result);
} /* ecma_proxy_object_prevent_extensions */

/**
 * The Proxy object [[GetOwnProperty]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.5
 *
 * Note: - Returned value is always a simple value so freeing it is unnecessary.
 *       - If the operation does not fail, freeing the filled property descriptor is the caller's responsibility
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE_FALSE} - depends on whether object has property with the given name
 */
ecma_value_t
ecma_proxy_object_get_own_property_descriptor (ecma_object_t *obj_p, /**< proxy object */
                                               ecma_string_t *prop_name_p, /**< property name */
                                               ecma_property_descriptor_t *prop_desc_p) /**< [out] property
                                                                                         *   descriptor */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));
  JERRY_UNUSED_3 (obj_p, prop_name_p, prop_desc_p);
  return ecma_raise_type_error (ECMA_ERR_MSG ("UNIMPLEMENTED: Proxy.[[GetOwnProperty]]"));
} /* ecma_proxy_object_get_own_property_descriptor */

/**
 * The Proxy object [[DefineOwnProperty]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.6
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE_FALSE} - depends on whether the property can be defined for the given object
 */
ecma_value_t
ecma_proxy_object_define_own_property (ecma_object_t *obj_p, /**< proxy object */
                                       ecma_string_t *prop_name_p, /**< property name */
                                       const ecma_property_descriptor_t *prop_desc_p) /**< property descriptor */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));
  JERRY_UNUSED_3 (obj_p, prop_name_p, prop_desc_p);
  return ecma_raise_type_error (ECMA_ERR_MSG ("UNIMPLEMENTED: Proxy.[[DefineOwnProperty]]"));
} /* ecma_proxy_object_define_own_property */

/**
 * The Proxy object [[HasProperty]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.7
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE_FALSE} - depends on whether the property is found
 */
ecma_value_t
ecma_proxy_object_has (ecma_object_t *obj_p, /**< proxy object */
                       ecma_string_t *prop_name_p) /**< property name */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 2. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 3-6. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_HAS);

  /* 7. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 8. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_op_object_has_property (target_obj_p, prop_name_p);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t prop_value = ecma_make_prop_name_value (prop_name_p);
  ecma_value_t args[] = {target, prop_value};

  /* 9. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, args, 2);

  ecma_deref_object (func_obj_p);

  /* 10. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  /* 11. */
  if (!boolean_trap_result)
  {
    ecma_property_descriptor_t target_desc;

    ecma_value_t status = ecma_op_object_get_own_property_descriptor (target_obj_p, prop_name_p, &target_desc);

    if (ECMA_IS_VALUE_ERROR (status))
    {
      return status;
    }

    if (ecma_is_value_true (status))
    {
      bool prop_is_configurable = target_desc.flags & ECMA_PROP_IS_CONFIGURABLE;

      ecma_free_property_descriptor (&target_desc);

      if (!prop_is_configurable)
      {
        return ecma_raise_type_error (ECMA_ERR_MSG ("Trap returned falsish for property which exists "
                                                    "in the proxy target as non-configurable"));
      }

      ecma_value_t extensible_target = ecma_builtin_object_object_is_extensible (target_obj_p);

      if (ECMA_IS_VALUE_ERROR (extensible_target))
      {
        return extensible_target;
      }

      if (ecma_is_value_false (extensible_target))
      {
        return ecma_raise_type_error (ECMA_ERR_MSG ("Trap returned falsish for property but "
                                                    "the proxy target is not extensible"));
      }
    }
  }

  /* 12. */
  return ecma_make_boolean_value (boolean_trap_result);
} /* ecma_proxy_object_has */

/**
 * The Proxy object [[Get]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.8
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         value of the given nameddata property or the result of the getter function call - otherwise
 */
ecma_value_t
ecma_proxy_object_get (ecma_object_t *obj_p, /**< proxy object */
                       ecma_string_t *prop_name_p, /**< property name */
                       ecma_value_t receiver) /**< receiver to invoke getter function */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 2. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 3-6. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_GET);

  /* 7. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 8. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_op_object_get_with_receiver (target_obj_p, prop_name_p, receiver);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t prop_value = ecma_make_prop_name_value (prop_name_p);
  ecma_value_t args[] = { target, prop_value, receiver };

  /* 9. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, args, 3);

  ecma_deref_object (func_obj_p);

  /* 10. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  /* 11. */
  ecma_property_descriptor_t target_desc;

  ecma_value_t status = ecma_op_object_get_own_property_descriptor (target_obj_p, prop_name_p, &target_desc);

  /* 12. */
  if (ECMA_IS_VALUE_ERROR (status))
  {
    return status;
  }

  /* 13. */
  if (ecma_is_value_true (status))
  {
    ecma_value_t ret_value = ECMA_VALUE_EMPTY;

    if ((target_desc.flags & ECMA_PROP_IS_VALUE_DEFINED)
        && !(target_desc.flags & ECMA_PROP_IS_CONFIGURABLE)
        && !(target_desc.flags & ECMA_PROP_IS_WRITABLE)
        && !ecma_op_same_value (trap_result, target_desc.value))
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("given property is a read-only and non-configurable"
                                                       " data property on the proxy target"));
    }
    else if (!(target_desc.flags & ECMA_PROP_IS_CONFIGURABLE)
            && (target_desc.flags & (ECMA_PROP_IS_GET_DEFINED | ECMA_PROP_IS_SET_DEFINED))
            && target_desc.get_p == NULL
            && !ecma_is_value_undefined (trap_result))
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("given property is a non-configurable property and"
                                                       " does not have a getter function"));
    }

    ecma_free_property_descriptor (&target_desc);

    if (ECMA_IS_VALUE_ERROR (ret_value))
    {
      ecma_free_value (trap_result);

      return ret_value;
    }
  }

  /* 14. */
  return trap_result;
} /* ecma_proxy_object_get */

/**
 * The Proxy object [[Set]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.9
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE/FALSE} - depends on whether the propety can be set to the given object
 */
ecma_value_t
ecma_proxy_object_set (ecma_object_t *obj_p, /**< proxy object */
                       ecma_string_t *prop_name_p, /**< property name */
                       ecma_value_t value, /**< value to set */
                       ecma_value_t receiver) /**< receiver to invoke setter function */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 2. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 3-6. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_SET);

  /* 7. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 8. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_op_object_put_with_receiver (target_obj_p, prop_name_p, value, receiver, false);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t prop_name_value = ecma_make_prop_name_value (prop_name_p);
  ecma_value_t args[] = { target, prop_name_value, value, receiver };

  /* 9. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, args, 4);

  ecma_deref_object (func_obj_p);

  /* 10. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  /* 11. */
  if (!boolean_trap_result)
  {
    return ECMA_VALUE_FALSE;
  }

  /* 12. */
  ecma_property_descriptor_t target_desc;

  ecma_value_t status = ecma_op_object_get_own_property_descriptor (target_obj_p, prop_name_p, &target_desc);

  /* 13. */
  if (ECMA_IS_VALUE_ERROR (status))
  {
    return status;
  }

  /* 14. */
  if (ecma_is_value_true (status))
  {
    ecma_value_t ret_value = ECMA_VALUE_EMPTY;

    if ((target_desc.flags & ECMA_PROP_IS_VALUE_DEFINED)
        && !(target_desc.flags & ECMA_PROP_IS_CONFIGURABLE)
        && !(target_desc.flags & ECMA_PROP_IS_WRITABLE)
        && !ecma_op_same_value (value, target_desc.value))
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("The property exists in the proxy target as a"
                                                       " non-configurable and non-writable data property"
                                                       " with a different value."));
    }
    else if (!(target_desc.flags & ECMA_PROP_IS_CONFIGURABLE)
            && (target_desc.flags & (ECMA_PROP_IS_GET_DEFINED | ECMA_PROP_IS_SET_DEFINED))
            && target_desc.set_p == NULL)
    {
      ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("The property exists in the proxy target as a"
                                                       " non-configurable accessor property whitout a setter."));
    }

    ecma_free_property_descriptor (&target_desc);

    if (ECMA_IS_VALUE_ERROR (ret_value))
    {
      return ret_value;
    }
  }

  /* 15. */
  return ECMA_VALUE_TRUE;
} /* ecma_proxy_object_set */

/**
 * The Proxy object [[Delete]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.10
 *
 * Note: Returned value is always a simple value so freeing it is unnecessary.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ECMA_VALUE_{TRUE/FALSE} - depends on whether the propety can be deleted
 */
ecma_value_t
ecma_proxy_object_delete_property (ecma_object_t *obj_p, /**< proxy object */
                                   ecma_string_t *prop_name_p) /**< property name */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 2. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 3-6.*/
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_DELETE_PROPERTY_UL);

  /* 7. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 8. */
  if (ecma_is_value_undefined (trap))
  {
    return ecma_op_object_delete (target_obj_p, prop_name_p, false);
  }

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t prop_name_value = ecma_make_prop_name_value (prop_name_p);
  ecma_value_t args[] = { target, prop_name_value };

  /* 9. */
  ecma_value_t trap_result = ecma_op_function_call (func_obj_p, handler, args, 2);

  ecma_deref_object (func_obj_p);

  /* 10. */
  if (ECMA_IS_VALUE_ERROR (trap_result))
  {
    return trap_result;
  }

  bool boolean_trap_result = ecma_op_to_boolean (trap_result);

  ecma_free_value (trap_result);

  /* 11. */
  if (!boolean_trap_result)
  {
    return ECMA_VALUE_FALSE;
  }

  /* 12. */
  ecma_property_descriptor_t target_desc;

  ecma_value_t status = ecma_op_object_get_own_property_descriptor (target_obj_p, prop_name_p, &target_desc);

  /* 13. */
  if (ECMA_IS_VALUE_ERROR (status))
  {
    return status;
  }

  /* 14. */
  if (ecma_is_value_false (status))
  {
    return ECMA_VALUE_TRUE;
  }

  ecma_value_t ret_value = ECMA_VALUE_TRUE;

  /* 15. */
  if (!(target_desc.flags & ECMA_PROP_IS_CONFIGURABLE))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Trap returned truish for property which is "
                                                     "non-configurable in the proxy target."));
  }

  ecma_free_property_descriptor (&target_desc);

  /* 16. */
  return ret_value;
} /* ecma_proxy_object_delete_property */

/**
 * The Proxy object [[Enumerate]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.11
 *
 * Note: Returned value must be freed with ecma_free_value.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         ecma-object - otherwise
 */
ecma_value_t
ecma_proxy_object_enumerate (ecma_object_t *obj_p) /**< proxy object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));
  JERRY_UNUSED (obj_p);
  return ecma_raise_type_error (ECMA_ERR_MSG ("UNIMPLEMENTED: Proxy.[[Enumerate]]"));
} /* ecma_proxy_object_enumerate */

/**
 * The Proxy object [[OwnPropertyKeys]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.12
 *
 * Note: If the returned collection is not NULL, it must be freed with
 *       ecma_collection_free if it is no longer needed
 *
 * @return NULL - if the operation fails
 *         pointer to a newly allocated list of property names - otherwise
 */
ecma_collection_t *
ecma_proxy_object_own_property_keys (ecma_object_t *obj_p) /**< proxy object */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));
  JERRY_UNUSED (obj_p);
  ecma_raise_type_error (ECMA_ERR_MSG ("UNIMPLEMENTED: Proxy.[[OwnPropertyKeys]]"));
  return NULL;
} /* ecma_proxy_object_own_property_keys */

/**
 * The Proxy object [[Call]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.13
 *
 * Note: Returned value must be freed with ecma_free_value.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         result of the function call - otherwise
 */
ecma_value_t
ecma_proxy_object_call (ecma_object_t *obj_p, /**< proxy object */
                        ecma_value_t this_argument, /**< this argument to invoke the function */
                        const ecma_value_t *args_p, /**< argument list */
                        ecma_length_t argc) /**< number of arguments */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t *proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 1. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 2-5.*/
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_APPLY);

  /* 6. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;

  /* 7. */
  if (ecma_is_value_undefined (trap))
  {
    ecma_object_t *target_obj_p = ecma_get_object_from_value (target);
    return ecma_op_function_call (target_obj_p, this_argument, args_p, argc);
  }

  /* 8. */
  ecma_value_t args_array = ecma_op_create_array_object (args_p, argc, false);
  ecma_value_t value_array[] = {target, this_argument, args_array};
  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  /* 9. */
  ecma_value_t ret_value = ecma_op_function_call (func_obj_p, handler, value_array, 3);
  ecma_deref_object (func_obj_p);
  ecma_deref_object (ecma_get_object_from_value (args_array));

  return ret_value;
} /* ecma_proxy_object_call */

/**
 * The Proxy object [[Construct]] internal routine
 *
 * See also:
 *          ECMAScript v6, 9.5.14
 *
 * Note: Returned value must be freed with ecma_free_value.
 *
 * @return ECMA_VALUE_ERROR - if the operation fails
 *         result of the construct call - otherwise
 */
ecma_value_t
ecma_proxy_object_construct (ecma_object_t *obj_p, /**< proxy object */
                             ecma_object_t *new_target_p, /**< new target */
                             const ecma_value_t *args_p, /**< argument list */
                             ecma_length_t argc) /**< number of arguments */
{
  JERRY_ASSERT (ECMA_OBJECT_IS_PROXY (obj_p));

  ecma_proxy_object_t * proxy_obj_p = (ecma_proxy_object_t *) obj_p;

  /* 1. */
  ecma_value_t handler = proxy_obj_p->handler;

  /* 2-5. */
  ecma_value_t trap = ecma_validate_proxy_object (handler, LIT_MAGIC_STRING_CONSTRUCT);

  /* 6. */
  if (ECMA_IS_VALUE_ERROR (trap))
  {
    return trap;
  }

  ecma_value_t target = proxy_obj_p->target;
  ecma_object_t *target_obj_p = ecma_get_object_from_value (target);

  /* 7. */
  if (ecma_is_value_undefined (trap))
  {
    JERRY_ASSERT (ecma_object_is_constructor (target_obj_p));

    return ecma_op_function_construct (target_obj_p, new_target_p, args_p, argc);
  }

  /* 8. */
  ecma_value_t arg_array = ecma_op_create_array_object (args_p, argc, false);

  ecma_object_t *func_obj_p = ecma_get_object_from_value (trap);
  ecma_value_t new_target_value = ecma_make_object_value (new_target_p);
  ecma_value_t function_call_args[] = {target, arg_array, new_target_value};

  /* 9. */
  ecma_value_t new_obj = ecma_op_function_call (func_obj_p, handler, function_call_args, 3);

  ecma_free_value (arg_array);
  ecma_deref_object (func_obj_p);

  /* 10 .*/
  if (ECMA_IS_VALUE_ERROR (new_obj))
  {
    return new_obj;
  }

  /* 11. */
  if (!ecma_is_value_object (new_obj))
  {
    ecma_free_value (new_obj);

    return ecma_raise_type_error (ECMA_ERR_MSG ("Trap returned non-object"));
  }

  /* 12. */
  return new_obj;
} /* ecma_proxy_object_construct */

#endif /* ENABLED (JERRY_ES2015_BUILTIN_PROXY) */

/**
 * @}
 * @}
 */
