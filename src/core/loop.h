#ifndef __LOOP_H
#define __LOOP_H

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

#endif