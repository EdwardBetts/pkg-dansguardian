// ListManager - contains the ListContainers for all item and phrase lists, and can create new ones

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@/ jadeb.com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


// INCLUDES
#ifdef HAVE_CONFIG_H
	#include "dgconfig.h"
#endif
#include "ListManager.hpp"

#include <syslog.h>
#include <ctime>
#include <sys/stat.h>


// GLOBALS

extern bool is_daemonised;


// IMPLEMENTATION

ListManager::~ListManager()
{
	for (unsigned int i = 0; i < l.size(); i++) {
		if (l[i] != NULL) {
			delete l[i];
			l[i] = NULL;
		}
	}
}

// find an unused list in our collection of lists
int ListManager::findNULL()
{
	for (unsigned int i = 0; i < l.size(); i++) {
		if (l[i] == NULL) {
#ifdef DGDEBUG
			std::cout << "found free list:" << i << std::endl;
#endif
			return (signed) i;
		}
	}
	return -1;
}

// delete all lists with zero reference count
void ListManager::garbageCollect()
{
	for (unsigned int i = 0; i < l.size(); i++) {
		if (l[i] != NULL) {
			if ((*l[i]).refcount < 1) {
#ifdef DGDEBUG
				std::cout << "deleting zero ref list: " << i << " " << l[i]->refcount << std::endl;
#endif
				delete l[i];
				l[i] = NULL;
			}
		}
	}
}

void ListManager::deRefList(size_t i)
{
	l[i]->refcount--;
#ifdef DGDEBUG
	std::cout << "de-referencing list ref: " << i << ", refcount: " << l[i]->refcount << " (" << l[i]->sourcefile << ")" << std::endl;
#endif
	for (size_t j = 0; j < l[i]->morelists.size(); ++j)
		deRefList(l[i]->morelists[j]);
}

void ListManager::refList(size_t i)
{
	l[i]->refcount++;
#ifdef DGDEBUG
	std::cout << "referencing list ref: " << i << ", refcount: " << l[i]->refcount << " (" << l[i]->sourcefile << ")" << std::endl;
#endif
	for (size_t j = 0; j < l[i]->morelists.size(); ++j)
		refList(l[i]->morelists[j]);
}

// load the given list, or increase refcount on list if it's already been loaded.
int ListManager::newItemList(const char *filename, bool startswith, int filters, bool parent)
{
	for (size_t i = 0; i < l.size(); i++) {
		if (l[i] == NULL) {
			continue;
		}
		if ((*l[i]).previousUseItem(filename, startswith, filters)) {
			// this upToDate check also checks all .Included files
			if ((*l[i]).upToDate()) {
#ifdef DGDEBUG
				std::cout << "Using previous item: " << i << " " << filename << std::endl;
#endif
				refList(i);
				return i;
			}
		}
	}
	// find an empty list slot, create a new listcontainer, and load the list
	int free = findNULL();
	if (free > -1) {
		l[free] = new ListContainer;
	} else {
#ifdef DGDEBUG
		std::cout << "pushing back for new list" << std::endl;
#endif
		l.push_back(new ListContainer);
		free = l.size() - 1;
	}
	(*l[free]).parent = parent;
	if (!(*l[free]).readItemList(filename, startswith, filters)) {
		delete l[free];
		l[free] = NULL;
		return -1;
	}
	return free;
}

// create a new phrase list. check dates on top-level list files to see if a reload is necessary.
// note: unlike above, doesn't automatically call readPhraseList.
// pass in exception, banned, and weighted phrase lists all at once.
int ListManager::newPhraseList(const char *exception, const char *banned, const char *weighted)
{
	time_t bannedpfiledate = getFileDate(banned);
	time_t exceptionpfiledate = getFileDate(exception);
	time_t weightedpfiledate = getFileDate(weighted);
	for (size_t i = 0; i < l.size(); i++) {
		if (l[i] == NULL) {
			continue;
		}
		if ((*l[i]).exceptionpfile == String(exception) && (*l[i]).bannedpfile == String(banned) && (*l[i]).weightedpfile == String(weighted)) {
			if (bannedpfiledate <= (*l[i]).bannedpfiledate && exceptionpfiledate <= (*l[i]).exceptionpfiledate && weightedpfiledate <= (*l[i]).weightedpfiledate) {
				// Known limitation - only weighted, exception, banned phrase
				// list checked for changes - not the included files.
				//
				//need to check all files that were included for phrase list
				//so when phrases read in in list container it needs to store
				//all the file names and if a single one has changed needs a
				//complete regenerate
#ifdef DGDEBUG
				std::cout << "Using previous phrase: " << exception << " - " << banned << " - " << weighted << std::endl;
#endif
				refList(i);
				return i;
			}
		}
	}
	int free = findNULL();
	if (free > -1) {
		l[(unsigned) free] = new ListContainer;
	} else {
		l.push_back(new ListContainer);
		free = l.size() - 1;
	}
	(*l[(unsigned) free]).parent = true;  // all phrase lists are parent as
	// there are no sub lists
	(*l[(unsigned) free]).bannedpfiledate = bannedpfiledate;
	(*l[(unsigned) free]).exceptionpfiledate = exceptionpfiledate;
	(*l[(unsigned) free]).weightedpfiledate = weightedpfiledate;
	(*l[(unsigned) free]).exceptionpfile = exception;
	(*l[(unsigned) free]).bannedpfile = banned;
	(*l[(unsigned) free]).weightedpfile = weighted;
	return (unsigned) free;
}
