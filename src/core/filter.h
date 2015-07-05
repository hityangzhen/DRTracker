#ifndef __CORE_FILTER_H
#define __CORE_FILTER_H

/**
 * Define address filters.
 */

#include <map>
#include "core/basictypes.h"
#include "core/sync.h"
#include "core/log.h"

class RegionFilter {
public:
	explicit RegionFilter(Mutex *lock):internalLock(lock) {}
	~RegionFilter() { delete internalLock; }

	void AddRegion(address_t addr,size_t size) { AddRegion(addr,size,true); }
	size_t RemoveRegion(address_t addr) { return RemoveRegion(addr,true); }
	bool Filter(address_t addr) { return Filter(addr,true); }

	void AddRegion(address_t addr,size_t size,bool locking);
	size_t RemoveRegion(address_t addr,bool locking); 
	bool Filter(address_t addr, bool locking);

private:
	Mutex *internalLock;
	std::map<address_t,size_t> addrRegionMap;
	DISALLOW_COPY_CONSTRUCTORS(RegionFilter);
};

#endif /* __CORE_FILTER_H */