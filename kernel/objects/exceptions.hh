#pragma once

extern "C" {

typedef enum {
	_URC_NO_REASON = 0,
	_URC_FOREIGN_EXCEPTION_CAUGHT = 1,
	_URC_FATAL_PHASE2_ERROR = 2,
	_URC_FATAL_PHASE1_ERROR = 3,
	_URC_NORMAL_STOP = 4,
	_URC_END_OF_STACK = 5,
	_URC_HANDLER_FOUND = 6,
	_URC_INSTALL_CONTEXT = 7,
	_URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

using uint64 = unsigned long;

struct _Unwind_Exception;

typedef void (*_Unwind_Exception_Cleanup_Fn) (_Unwind_Reason_Code reason, struct _Unwind_Exception *exc);

struct _Unwind_Exception {
    uint64			 exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uint64			 private_1;
    uint64			 private_2;
};

struct _Unwind_Context {
};

_Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Exception *exception_object);

typedef _Unwind_Reason_Code (*_Unwind_Stop_Fn) 
        (int version,
		 _Unwind_Action actions,
		 uint64 exceptionClass,
		 struct _Unwind_Exception *exceptionObject,
		 struct _Unwind_Context *context,
		 void *stop_parameter );

_Unwind_Reason_Code _Unwind_ForcedUnwind
	    ( struct _Unwind_Exception *exception_object,
		_Unwind_Stop_Fn stop,
		void *stop_parameter );

void _Unwind_Resume (struct _Unwind_Exception *exception_object);

void _Unwind_DeleteException
	      (struct _Unwind_Exception *exception_object);

uint64 _Unwind_GetGR
	    (struct _Unwind_Context *context, int index);

void _Unwind_SetGR
	  (struct _Unwind_Context *context,
	   int index,
	   uint64 new_value);

uint64 _Unwind_GetIP
	    (struct _Unwind_Context *context);

void _Unwind_SetIP
	    (struct _Unwind_Context *context,
	     uint64 new_value);

uint64 _Unwind_GetLanguageSpecificData
	    (struct _Unwind_Context *context);

uint64 _Unwind_GetRegionStart
	    (struct _Unwind_Context *context);

_Unwind_Reason_Code (*__personality_routine)
	    (int version,
	     _Unwind_Action actions,
	     uint64 exceptionClass,
	     struct _Unwind_Exception *exceptionObject,
	     struct _Unwind_Context *context);


};