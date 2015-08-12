/**
 * Embeded Python in CPP.
 */
#include <python2.7/Python.h>
#include <iostream>
#include <sstream>
#include <assert.h>

std::string syncRunPythonScript(const std::string& script, const std::string& params)
{
  assert (Py_IsInitialized());

  if ( -1 != PyRun_SimpleString(script.c_str()) ) {
    PyObject* pMainModule = PyImport_AddModule("__main__"); 

    PyObject* pRunOnClientFunc = PyObject_GetAttrString(pMainModule, "run");
    if (pRunOnClientFunc && PyCallable_Check(pRunOnClientFunc)) {
      PyObject* pParamTuple = PyTuple_New(1);
      PyObject* pParamsString = PyString_FromString(params.c_str());
      PyTuple_SetItem(pParamTuple, 0, pParamsString); 
      PyObject* pResult = PyObject_CallObject(pRunOnClientFunc, pParamTuple);
      std::string ret;
      if (PyUnicode_Check(pResult)) {
        PyObject* temp_bytes = PyUnicode_AsUTF8String(pResult);
        if(temp_bytes) {
          ret = PyBytes_AS_STRING(temp_bytes);
        } else {
          assert(false);
        }
      } else if (PyString_Check(pResult)) {
        ret = PyString_AsString(pResult);
      } else {
        assert(false);
      }
      return ret;
    }
  }
  return "";
}

int main(void)
{
  Py_Initialize();

  printf("Py_IsInitialized, %d\n", Py_IsInitialized());

  std::ostringstream oss;
  oss << "def run(name):" << std::endl;
  oss << "  name = unicode(name, encoding='utf-8')" << std::endl;
  oss << "  return name.lower()" << std::endl;
  std::cout << syncRunPythonScript(oss.str(), "\xd0\xa2\xd0\x90\xd0\x9d\xd0\x93\xd0\x9e") << std::endl;

  PyGILState_Ensure();
  Py_Finalize();
  return 0;
}
