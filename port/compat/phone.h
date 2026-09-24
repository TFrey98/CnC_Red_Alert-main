/*
**	phone.h -- stub for the modem phone-list manager.
**
**	Absent from EA's source release, along with the Greenleaf library it drove
**	(see modem.h). CODE/function.h includes it unconditionally, so the header
**	must exist for every translation unit in the game.
**
**	Empty is sufficient for the single-player path, and that is not a guess:
**	the only in-scope consumer, CODE/SESSION.H, forward-declares
**	`class PhoneEntryClass;` itself (line 53) and stores only
**	DynamicVectorClass<PhoneEntryClass *> -- pointers to an incomplete type,
**	which is well formed. DialMethodType, the other name one might expect from
**	here, is defined in SESSION.H too.
**
**	The files that need the real definition -- NULLMGR.H, NULLMGR.CPP and
**	NULLDLG.CPP -- are all on the modem multiplayer path and out of scope. They
**	will fail with a clear "incomplete type" at the point of use if that ever
**	changes, which is the intended signal.
*/
#ifndef WWPORT_COMPAT_PHONE_H
#define WWPORT_COMPAT_PHONE_H
#endif
