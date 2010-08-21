/*
 * Copyright (c) 1999-2004 Sourceforge JACOB Project.
 * All rights reserved. Originator: Dan Adler (http://danadler.com).
 * Get more information about JACOB at http://sourceforge.net/projects/jacob-project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "EventProxy.h"
#include "Variant.h"

#define EVENT_PROXY_DEBUG 0

// hook myself up as a listener for delegate
EventProxy::EventProxy(JNIEnv *env, 
		jobject aSinkObj, 
        CComPtr<IConnectionPoint> pConn,
        IID eid, 
        CComBSTR mName[], 
        DISPID mID[], 
        int mNum) :
   // initialize some variables
   m_cRef(0), pCP(pConn), 
   eventIID(eid), MethNum(mNum), MethName(mName), 
   MethID(mID)
{
	// keep a pointer to the sink
	javaSinkObj = env->NewGlobalRef(aSinkObj);
		if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}

	// we need this to attach to the event invocation thread
	env->GetJavaVM(&jvm);
		if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
	AddRef();
    Connect(env);
}

void EventProxy::Connect(JNIEnv *env) {
  HRESULT hr = pCP->Advise(this, &dwEventCookie);
  if (SUCCEEDED(hr)) {
        connected = 1;
  } else {
        connected = 0;
    ThrowComFail(env, "Advise failed", hr);
  }
}

// unhook myself up as a listener and get rid of delegate
EventProxy::~EventProxy()
{
	JNIEnv *env;
    Disconnect();
    jint vmConnectionStatus = JNI_EVERSION ;
    jint attachReturnStatus = -1; // AttachCurrentThread return status.. negative numbers are failure return codes.
    
	// attach to the current running thread -- JDK 1.4 jni.h has two param cover for 3 param call
    vmConnectionStatus = jvm->GetEnv((void **)&env, JNI_VERSION_1_2);
		if ((env != NULL)&& env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
   	if (vmConnectionStatus == JNI_EDETACHED){
		//printf("Unhook: Attaching to current thread using JNI Version 1.2 (%d)\n",vmConnectionStatus);
		JavaVMAttachArgs attachmentArgs; 
        attachmentArgs.version = JNI_VERSION_1_2;  
        attachmentArgs.name = NULL; 
        attachmentArgs.group = NULL; 
        attachReturnStatus = jvm->AttachCurrentThread((void **)&env, &attachmentArgs); 
		if ((env != NULL) && env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
   	} else {
   		// should really look for JNI_OK versus an error because it could have been JNI_EVERSION
   		// started method with thread hooked to VM so no need to attach again
   		//printf("Unhook:  No need to attach because already attached %d\n",vmConnectionStatus);
   	}

   	// we should always have an env by this point but lets be paranoid and check
	if (env != NULL){
		env->DeleteGlobalRef(javaSinkObj);
			if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
	}
	if (MethNum) {
	    delete [] MethName;
	    delete [] MethID;
	}
	// detach from thread only if we attached to it in this function
	if (attachReturnStatus == 0){
		jvm->DetachCurrentThread();
		//printf("Unhook: Detached\n");
	} else {
		//printf("Unhook: No need to detatch because attached prior to method\n");
	}
  	//fflush(stdout);
}

void EventProxy::Disconnect() {
    if (connected) {
        pCP->Unadvise(dwEventCookie);
    }
}

// I only support the eventIID interface which was passed in
// by the DispatchEvent wrapper who looked it up as the
// source object's default source interface
STDMETHODIMP EventProxy::QueryInterface(REFIID rid, void **ppv)
{
  if (rid == IID_IUnknown || rid == eventIID || rid == IID_IDispatch) 
  {
    *ppv = this;
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

// This should never get called - the event source fires events
// by dispid's, not by name
STDMETHODIMP EventProxy::GetIDsOfNames(REFIID riid,
    OLECHAR **rgszNames, UINT cNames, LCID lcid, DISPID *rgDispID)
{
  return E_UNEXPECTED;
}

// The actual callback from the connection point arrives here
STDMETHODIMP EventProxy::Invoke(DISPID dispID, REFIID riid,
    LCID lcid, unsigned short wFlags, DISPPARAMS *pDispParams,
    VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr) {
 const char *eventMethodName = NULL; //Sourceforge report 1394001
 JNIEnv *env = NULL;

 if (EVENT_PROXY_DEBUG) fprintf(stderr, "In invoke\n");
 // map dispID to jmethodID
 for (int i = 0; i < MethNum; i++) {
    if (MethID[i] == dispID) {
       USES_CONVERSION;
       eventMethodName = W2A((OLECHAR *) MethName[i]);
    }
 }

 // added 1.12 - Just bail if can't find signature.  no need to attach
 if (!eventMethodName) return S_OK;
 if (EVENT_PROXY_DEBUG) fprintf(stderr, "In invoke of %s\n", eventMethodName);

 if (DISPATCH_METHOD & wFlags) {
    // attach to the current running thread
    JavaVMAttachArgs attachmentArgs;
    attachmentArgs.version = JNI_VERSION_1_2;
    attachmentArgs.name = NULL;
    attachmentArgs.group = NULL;

    jvm->AttachCurrentThread((void **) &env, &attachmentArgs);
    if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}

    // find the class of the InvocationHandler
    jclass javaSinkClass = env->GetObjectClass(javaSinkObj);
    if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}

    jmethodID invokeMethod = env->GetMethodID(javaSinkClass, "invoke", "(Ljava/lang/String;[Lcom/jacob/com/Variant;)Lcom/jacob/com/Variant;");
    if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}

    jstring eventMethodNameAsString = env->NewStringUTF(eventMethodName);

    // create the variant parameter array
    // how many params
    int numVariantParams = pDispParams->cArgs;
    if (EVENT_PROXY_DEBUG) fprintf(stderr, "Setup parm list for event %s(%d)\n", eventMethodName, numVariantParams);
    // make an array of them
    jobjectArray varr = env->NewObjectArray(numVariantParams, VARIANT_CLASS, 0);
    if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}

    int i,j;
    for (i=numVariantParams-1,j=0; i>=0; i--,j++) {
       env->SetObjectArrayElement(varr, j, createVariant(env, &pDispParams->rgvarg[i]));
       if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
    }

    // Set up the return value
    jobject ret = env->CallObjectMethod(javaSinkObj, invokeMethod, eventMethodNameAsString, varr);
    if (env->ExceptionOccurred()) { env->ExceptionDescribe(); env->ExceptionClear();}
    if (ret != NULL) populateVariant(env, ret, pVarResult);

    // Begin code from Jiffie team that copies parameters back from java to COM
    for(i=numVariantParams-1,j=0;i>=0;i--,j++) {
       jobject arg = env->GetObjectArrayElement(varr, j);
       populateVariant(env, arg, &pDispParams->rgvarg[i]);
       env->DeleteLocalRef(arg);
    }
    // End code from Jiffie team that copies parameters back from java to COM
    
    jvm->DetachCurrentThread();     // detach from thread
    
    return S_OK;
  }
  return E_NOINTERFACE;
}
