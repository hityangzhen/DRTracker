#include "core/filter.h"


void RegionFilter::AddRegion(address_t addr,size_t size,bool locking)
{
	ScopedLock slock(internalLock,locking);
	addrRegionMap[addr]=size;
}

size_t RegionFilter::RemoveRegion(address_t addr,bool locking)
{
	ScopedLock slock(internalLock,locking);
	if(!addr) return 0;
	size_t size=0;
	std::map<address_t,size_t>::iterator it=addrRegionMap.find(addr);
	if(it!=addrRegionMap.end()) {
		size=it->second;
		addrRegionMap.erase(it);
	}
	return size;
}
//Addresses not in region should be filtered
bool RegionFilter::Filter(address_t addr,bool locking)
{
	ScopedLock slock(internalLock,locking);

	if(addrRegionMap.begin()==addrRegionMap.end())
		return true;
	std::map<address_t,size_t>::iterator it=addrRegionMap.upper_bound(addr);
	if(it==addrRegionMap.begin())
		return true;

	it--;
	address_t regionStart=it->first;
	size_t regionSize=it->second;
	if(addr>=regionStart && addr<regionStart+regionSize)
		return false;
	return true;
}