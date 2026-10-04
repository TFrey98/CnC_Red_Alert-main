/*
**	phone.h -- the modem phone book entry, RECONSTRUCTED for the arm64 port.
**
**	CODE/PHONE.H was not part of EA's release. This was an empty stub while no
**	compiled file needed the class; SESSION.CPP does (it saves and loads the phone
**	book), and SESSION.CPP is core single-player state -- `Session.Type` decides
**	whether a game is single-player at all -- so the class is reconstructed here.
**
**	Every member below is taken from its uses in SESSION.CPP, NULLDLG.CPP and
**	NULLMGR.CPP: Name and Number as character arrays bounded by PHONE_MAX_NAME /
**	PHONE_MAX_NUM, and Settings as SESSION.H's SerialSettingsType. The two sizes
**	(21) follow Tiberian Dawn's PHONE.H from the same engine family, from memory --
**	treat them as reconstructed. They only bound modem-dialog text; the phone book
**	is stored as INI text, so there is no binary layout to match.
*/
#ifndef WWPORT_COMPAT_PHONE_H
#define WWPORT_COMPAT_PHONE_H

#include "session.h"

class PhoneEntryClass {
	public:
		enum PhoneEntryEnum {
			PHONE_MAX_NAME = 21,
			PHONE_MAX_NUM = 21
		};

		char               Name[PHONE_MAX_NAME];
		char               Number[PHONE_MAX_NUM];
		SerialSettingsType Settings;
};

#endif
