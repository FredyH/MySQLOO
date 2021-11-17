#ifndef _MySQL_H
#define _MySQL_H
#include <cstdint>
//Source: http://stackoverflow.com/a/25510879
//Pretty much an action that is always going to execute when the 
//scope it was declared in is left (so exceptions/returns/break/etc.)
template <typename F>
struct FinalAction {
	explicit FinalAction(F f) : clean_{ f } {}
	~FinalAction() { clean_(); }
	F clean_;
};
template <typename F>
FinalAction<F> finally(F f) {
	return FinalAction<F>(f);
}
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <windows.h>
#endif
#include <mysql.h>

#endif