#pragma once
#include <stdint.h>
#include <stddef.h>

class IShellCallback{
	public:
	virtual int printChar(char c)=0;
	virtual int printf(const char *fmt, ...)=0;
	virtual bool IsPrivilegedUser()=0;
	virtual const char* GetUsername()=0;
	IShellCallback(){}
	virtual ~IShellCallback(){}
};

typedef uint32_t ShellId;

class IShellHandler{
	public:
	virtual ShellId beginShell(IShellCallback* cb)=0;
	virtual int handleChars(const char* chars, size_t len, ShellId shellId, IShellCallback* cb )=0;
	virtual void endShell(ShellId shellId, IShellCallback* cb)=0;
	int GetMagicNumber(){
		return 42;
	}
};


	
