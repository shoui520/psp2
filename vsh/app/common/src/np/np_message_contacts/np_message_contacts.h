/*
 * SceNpMessageContacts's exports (vs0:sys/external/np_message_contacts.suprx) that the
 * invitation dialog calls: contacts_list_plugin's module functions, and the contacts database's
 * open and close. No Sony name is known for any of them; what each does is read from its caller.
 */
#ifndef NP_MESSAGE_CONTACTS_H
#define NP_MESSAGE_CONTACTS_H

#include <scetypes.h>

namespace paf {
class Plugin;
}

extern "C" {

/* contacts_list_plugin's init, start, stop and exit functions */
void SceNpMessageContacts_0212231C(paf::Plugin *plugin);
void SceNpMessageContacts_812F9D28(paf::Plugin *plugin);
void SceNpMessageContacts_92636117(paf::Plugin *plugin);
void SceNpMessageContacts_88E9872C(paf::Plugin *plugin);

/* the contacts database: opened from the friends database's path, in four steps */
int SceNpMessageContacts_017D299D(void);
int SceNpMessageContacts_3E7B69AE(const char *path);
int SceNpMessageContacts_E5E42852(void);
int SceNpMessageContacts_2117B92A(void);

/* and closed, in four steps */
int SceNpMessageContacts_FA2CA5A5(void);
int SceNpMessageContacts_44B59B98(void);
int SceNpMessageContacts_C157920F(void);
int SceNpMessageContacts_21B462D1(void);

}

#endif
