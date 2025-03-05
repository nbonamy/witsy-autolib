#include <node_api.h>
#include <string.h>
#include <math.h>
#include "keysender.h"
#include "process.h"
#include "window.h"

#define MAX_PATH_LENGTH 260

static napi_value SendCtrlKeyWrapper(napi_env env, napi_callback_info info)
{
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  char buffer[32]; // Sufficient for key characters
  size_t buffer_size = sizeof(buffer);

  // Get the arguments
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok || argc < 1)
  {
    napi_throw_error(env, NULL, "Expected a string argument");
    return NULL;
  }

  // Get the string value
  status = napi_get_value_string_utf8(env, args[0], buffer, buffer_size, NULL);
  if (status != napi_ok)
  {
    napi_throw_error(env, NULL, "Expected a string argument");
    return NULL;
  }

  // Call the function and create return value
  uint32_t result = SendCtrlKey(buffer);
  napi_value return_val;
  napi_create_uint32(env, result, &return_val);

  return return_val;
}

static napi_value GetForemostWindowWrapper(napi_env env, napi_callback_info info)
{
  napi_status status;
  
  // Get the foremost window information
  ForemostWindowInfo* windowInfo = GetForemostWindow();
  if (!windowInfo) {
    napi_value result;
    status = napi_get_null(env, &result);
    return result;
  }
  
  // Create result object
  napi_value result;
  status = napi_create_object(env, &result);
  
  // Add exePath property
  if (windowInfo->exePath) {
    napi_value exePath;
    status = napi_create_string_utf8(env, windowInfo->exePath, strlen(windowInfo->exePath), &exePath);
    status = napi_set_named_property(env, result, "exePath", exePath);
  }
  
  // Add title property
  if (windowInfo->title) {
    napi_value title;
    status = napi_create_string_utf8(env, windowInfo->title, strlen(windowInfo->title), &title);
    status = napi_set_named_property(env, result, "title", title);
  }
  
  // Add productName property
  if (windowInfo->productName) {
    napi_value productName;
    status = napi_create_string_utf8(env, windowInfo->productName, strlen(windowInfo->productName), &productName);
    status = napi_set_named_property(env, result, "productName", productName);
  }
  
  // Add processId property
  napi_value processId;
  status = napi_create_int32(env, windowInfo->processId, &processId);
  status = napi_set_named_property(env, result, "processId", processId);
  
  // Free the window info
  FreeWindowInfo(windowInfo);
  
  return result;
}

// Add this function after your other wrapper functions
static napi_value GetProductNameWrapper(napi_env env, napi_callback_info info)
{
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  char exePath[MAX_PATH_LENGTH];
  size_t exePathLength = 0;
  
  // Get the arguments (exe path)
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok || argc < 1) {
    napi_throw_error(env, NULL, "Expected a string argument (exePath)");
    return NULL;
  }
  
  // Get the string value of exe path
  status = napi_get_value_string_utf8(env, args[0], exePath, MAX_PATH_LENGTH, &exePathLength);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Expected a string argument for exePath");
    return NULL;
  }
  
  // Buffer for product name
  char productName[256] = {0};
  
  // Get product name from exe
  BOOL result = GetProductNameFromExe(exePath, productName, sizeof(productName));
  
  // If successful, return the product name as a string
  if (result) {
    napi_value name_value;
    status = napi_create_string_utf8(env, productName, strlen(productName), &name_value);
    return name_value;
  } else {
    // If failed, return null
    napi_value null_value;
    status = napi_get_null(env, &null_value);
    return null_value;
  }
}

static napi_value GetApplicationIconWrapper(napi_env env, napi_callback_info info)
{
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  char exePath[MAX_PATH_LENGTH];
  size_t exePathLength = 0;
  
  // Get the arguments (exe path)
  status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
  if (status != napi_ok || argc < 1) {
    napi_throw_error(env, NULL, "Expected a string argument (exePath)");
    return NULL;
  }
  
  // Get the string value of exe path
  status = napi_get_value_string_utf8(env, args[0], exePath, MAX_PATH_LENGTH, &exePathLength);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Expected a string argument for exePath");
    return NULL;
  }
  
  // Create result object
  napi_value result;
  status = napi_create_object(env, &result);
  
  // Get icon from the exe path
  IconData* iconData = GetIconFromExePath(exePath);
  if (iconData && iconData->data && iconData->size > 0) {
    // Add icon data as Buffer
    napi_value iconBuffer;
    void* iconBufferData;
    
    status = napi_create_buffer(env, iconData->size, &iconBufferData, &iconBuffer);
    if (status == napi_ok) {
      memcpy(iconBufferData, iconData->data, iconData->size);
      status = napi_set_named_property(env, result, "iconData", iconBuffer);
      
      // Add icon dimensions
      napi_value iconWidth, iconHeight;
      status = napi_create_int32(env, iconData->width, &iconWidth);
      status = napi_set_named_property(env, result, "iconWidth", iconWidth);
      status = napi_create_int32(env, iconData->height, &iconHeight);
      status = napi_set_named_property(env, result, "iconHeight", iconHeight);
    }
    
    // Free the icon data
    FreeIconData(iconData);
  } else {
    // No icon data, return empty object
    status = napi_set_named_property(env, result, "iconData", NULL);
    napi_value iconWidth, iconHeight;
    status = napi_create_int32(env, 0, &iconWidth);
    status = napi_set_named_property(env, result, "iconWidth", iconWidth);
    status = napi_create_int32(env, 0, &iconHeight);
    status = napi_set_named_property(env, result, "iconHeight", iconHeight);
  }
  
  return result;
}

static napi_value Init(napi_env env, napi_value exports)
{
  napi_value result;
  napi_create_object(env, &result);
  
  // Export sendCtrlKey
  napi_value send_ctrl_key_fn;
  napi_create_function(env, NULL, 0, SendCtrlKeyWrapper, NULL, &send_ctrl_key_fn);
  napi_set_named_property(env, result, "sendCtrlKey", send_ctrl_key_fn);
  
  // Export getForemostWindow
  napi_value get_foremost_window_fn;
  napi_create_function(env, NULL, 0, GetForemostWindowWrapper, NULL, &get_foremost_window_fn);
  napi_set_named_property(env, result, "getForemostWindow", get_foremost_window_fn);
  
  // Export getProductName
  napi_value get_product_name_fn;
  napi_create_function(env, NULL, 0, GetProductNameWrapper, NULL, &get_product_name_fn);
  napi_set_named_property(env, result, "getProductName", get_product_name_fn);

  // Export getApplicationIcon
  napi_value get_application_icon_fn;
  napi_create_function(env, NULL, 0, GetApplicationIconWrapper, NULL, &get_application_icon_fn);
  napi_set_named_property(env, result, "getApplicationIcon", get_application_icon_fn);

  return result;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)