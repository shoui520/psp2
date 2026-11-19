/*
 * applauncher_mode.cpp: ModeBase, and the manager of a tree of modes, updated on the main
 * thread (0x810C2100-0x810C236E). The file's name and the manager's names are ours
 * (PLACEHOLDER); ModeBase is Sony's.
 */
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include <common/main_thread_call_list.h>

using namespace paf;

/* (0x81544288) */
const char *ModeBase::s_className = "ModeBase";

/* (0x810C2100) */
const char *ModeBase::GetName()
{
	return s_className;
}

/* (0x810C210C) */
ModeBase::ModeBase()
{
}

/* (0x810C210C, 0x810C211A) */
ModeBase::~ModeBase()
{
}

/* the manager's modes: one slot each, none made yet; updated on every main thread tick
   (0x810C2130) */
ModeManager::ModeManager(const ModeTable &table) : ModeTable(table)
{
	m_nextMode = MODE_MANAGER_NONE;
	m_currentMode = MODE_MANAGER_NONE;
	m_previousMode = MODE_MANAGER_NONE;
	if (count != 0)
		m_modes.reserve(count);
	for (int i = 0; i < count; i++)
		m_modes.push_back(NULL);
	common::MainThreadCallList::Register(UpdateCallback, this);
}

/* the mode asked for made current: going to MODE_MANAGER_EXIT deletes the current mode and
   its parents; otherwise the current mode and its parents up to the one the new mode shares
   are deleted, and the modes from there down to the new one made (0x810C21DA) */
void ModeManager::Update()
{
	int next = m_nextMode;
	int mode = m_currentMode;

	if (mode != next) {
		if (next == MODE_MANAGER_EXIT) {
			do {
				if (m_modes[mode] != NULL) {
					delete m_modes[mode];
					m_modes[mode] = NULL;
				}
				mode = entries[mode].parent;
			} while (mode >= 0);
		} else {
			if (mode != MODE_MANAGER_NONE) {
				int leave = 1;

				for (;;) {
					int up = next;

					do {
						if (mode == up)
							leave = 0;
						up = entries[up].parent;
					} while (up != MODE_MANAGER_NONE);
					if (leave == 0)
						break;
					if (m_modes[mode] != NULL) {
						delete m_modes[mode];
						m_modes[mode] = NULL;
					}
					mode = entries[mode].parent;
				}
			}
			while (mode != next) {
				int child = next;

				for (int up = entries[next].parent; mode != up;
				     up = entries[up].parent)
					child = up;
				mode = child;
				if (m_modes[mode] == NULL)
					m_modes[mode] = entries[mode].create();
			}
		}
		int previous = m_currentMode;

		m_currentMode = next;
		m_previousMode = previous;
	}
	OnUpdate();
}

/* every mode deleted (0x810C22CC, 0x810C2316) */
ModeManager::~ModeManager()
{
	common::MainThreadCallList::Unregister(UpdateCallback, this);
	ChangeMode(MODE_MANAGER_EXIT);
	Update();
}

/* (0x810C2366) */
void ModeManager::UpdateCallback(void *manager)
{
	((ModeManager *)manager)->Update();
}
