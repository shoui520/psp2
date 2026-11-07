/*
 * ScePartyMemberListPlugin's exports (vs0:sys/external/party_member_list.suprx): the module
 * functions of party_memberlist_plugin, which the invitation dialog loads. No Sony name is known
 * for them.
 */
#ifndef PARTY_MEMBER_LIST_H
#define PARTY_MEMBER_LIST_H

namespace paf {
class Plugin;
}

extern "C" {

/* party_memberlist_plugin's init, start, stop and exit functions */
void ScePartyMemberListPlugin_948AF6E3(paf::Plugin *plugin);
void ScePartyMemberListPlugin_8DA476F3(paf::Plugin *plugin);
void ScePartyMemberListPlugin_CA3A2524(paf::Plugin *plugin);
void ScePartyMemberListPlugin_852B695D(paf::Plugin *plugin);

}

#endif
