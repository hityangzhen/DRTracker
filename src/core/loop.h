#ifndef __LOOP_H
#define __LOOP_H

#include <tr1/unordered_set>
class Loop {
public:
	Loop(int sl,int el):start_line_(sl),end_line_(el) {}
	~Loop() {}
	bool InLoop(int line) {
		return line>=start_line_ && line<=end_line_;
	}
protected:
	int start_line_;
	int end_line_;
};

//extend loop with the exiting conditions
class XLoop:public Loop{
public:
	XLoop(int sl,int el):Loop(sl,el) {}
	~XLoop() {}
	void AddExitingCondLine(int line) {
		exiting_cond_line_set_.insert(line);
	}
	bool ExitingCondLine(int line) {
		return exiting_cond_line_set_.find(line)!=exiting_cond_line_set_.end();
	}
protected:
	std::tr1::unordered_set<int> exiting_cond_line_set_;
};

#endif