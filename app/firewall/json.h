#ifndef _M_JSON_H_
#define _M_JSON_H_

/** simple encapsulation of lib json-c
 * */

#include <json-c/json.h>

/** Read content from a json file to json string, be careful,
 * the memory must be free after use by the caller, see JS_RREE()
 * @param path
 *  path of json file
 * @param name
 *  name of json file
 * @return
 *  json string pointer on success, NULL for a failure
 * */
char *JS(const char *path, const char *name);


/** Free json string alloc by JS()
 * @param js
 *  json string alloc by JS()
 * */
void JS_FREE(char *js);


/** Get json root from given json string, be careful, the memory 
 * must be free after use by the caller, see JR_FREE()
 * @param
 *  the given json string
 * @return
 *  json root pointer on success, NULL for a failure
 * */
json_object *JR(const char *js);


/** Free json root alloc by JR()
 * @param
 *  json root alloc by JR()
 * */
void JR_FREE(json_object *jr);


/** Get json array from given json root
 * @param jr
 *  the given json root
 * @param tag
 *  tag of which json array to read
 * @param ja
 *  json array pointer
 * @return
 *  number of json object in json array, -1 for a failure
 * */
int JA(json_object *jr, const char *tag, json_object **ja);


/** Get json object from given json array 
 * @param ja
 *  the given json array
 * @param index
 *  index of json object to get
 * @return
 *  json object pointer on success, NULL for a failure
 * */
json_object *JO(json_object *ja, int index);


/** Get json value from given json object
 * @param jo
 *  the given json object
 * @param tag
 *  tag of which json value to read
 * @return
 *  json value pointer on success, NULL for a failure
 * */
json_object *JV(json_object *jo, const char *tag);


/** Get value from JV in various type
 * */
int JV_I(json_object *jv);
const char *JV_S(json_object *jv);

#endif

// file format utf-8
// ident using space