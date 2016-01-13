#!/usr/bin/env python

"""
divided the sorted static profile into groups
and all potential stmt pairs in a group are non-interferent
"""

import sys
import os
import shutil

groups=[]

class PStmt(object):
	"""
	attributes:
		fn:	file name
		l:  line
		vc: vector clock
		ec: execution count
	"""
	def __init__(self,fn,l,vc,ec):
		self.fn_=fn
		self.l_=l
		self.vc_=vc
		self.ec_=ec

	def happens_before(self,pstmt2):
		"""
		use the vector clock and execution count to
		check the happens before relation
		"""
		same=True
		for thd in self.vc_:
			if thd not in pstmt2.vc_:
				ts2=0
			else:
				ts2=pstmt2.vc_.get(thd)
			ts1=self.vc_.get(thd)
			if ts1>ts2:
				return False
			elif ts1<ts2:
				same=False
		if not same:
			return True
		return self.ec_<pstmt2.ec_

class PStmtPair(object):
	"""
	a pair of two pstmts
	"""
	def __init__(self,pstmt1,pstmt2):
		self.pstmt1_=pstmt1
		self.pstmt2_=pstmt2

	def non_interferent_with(self,pstmtpair2):
		"""
		whether pstmts of current pstmtpair is not interferent with
		pstmts of pstmtpair2
		"""
		return (self.pstmt1_.happens_before(pstmtpair2.pstmt1_)
			and self.pstmt2_.happens_before(pstmtpair2.pstmt2_)) or \
			(self.pstmt2_.happens_before(pstmtpair2.pstmt1_)
			and self.pstmt1_.happens_before(pstmtpair2.pstmt2_))

	def same_line(self,pstmtpair2):
		"""
		whether pstmts of current pstmtpair have the same line 
		with the pstmts of pstmtpair2
		"""
		return self.pstmt1_.l_==pstmtpair2.pstmt1_.l_ \
		or self.pstmt1_.l_==pstmtpair2.pstmt2_.l_ \
		or self.pstmt2_.l_==pstmtpair2.pstmt1_.l_ \
		or self.pstmt2_.l_==pstmtpair2.pstmt2_.l_

def create_pstmt(items):
	"""
	construct a PStmt obj
	"""
	print items
	vc={}
	clks=items[2].split(',')
	for clk in clks:

		if clk.strip()=='':
			continue
		clk_items=clk.split(':')
		# thread:timestamp
		vc[clk_items[0]]=int(clk_items[1])
	return PStmt(items[0],items[1],vc,int(items[3]))

def sort_group():
	"""
	sort the group according to the group size
	"""
	def size_compare(g1,g2):
		return -(len(g1)-len(g2))
	groups.sort(size_compare)

def valid_group(group,pstmtpair2):
	"""
	whether the pstmt pairs in group are interferent free
	"""
	for pp in group:
		if pp.same_line(pstmtpair2):
			return False
		elif pp.non_interferent_with(pstmtpair2) or \
			pstmtpair2.non_interferent_with(pp):
			continue
		else:
			return False
	return True

def group(infile_name):
	infile=open(infile_name)
	while True:
		lines=infile.readlines(10000)
		if not lines:
			break
		for line in lines:
			items=line.split(' ')
			valid=True
			for item in items:
				if item=='':
					valid=False
					break
			if not valid:
				continue
			pstmt1=create_pstmt(items[:4])
			pstmt2=create_pstmt(items[4:])
			pp=PStmtPair(pstmt1,pstmt2)
			# whether find the appropriate group
			flag=False
			# sort the groups
			sort_group()
			for g in groups:
				if valid_group(g,pp):
					g.append(pp)
					flag=True
					break
			# do not find, add a new group
			if not flag:
				groups.append([pp])
	infile.close()

def export(outfile_name):
	"""
	export each group pstmt pairs into files
	"""
	# remove the older dir and files
	if not os.path.exists(outfile_name):
		os.mkdir(outfile_name)
	else:
		shutil.rmtree(outfile_name)

	if not outfile_name.endswith('/'):
		outfile_name.append('/')

	for id,g in enumerate(groups):
		outfile=open(outfile_name+'g'+str(id)+'.out','w')
		lines=[]
		for pp in g:
			s='%s %s %s %s\n' % (pp.pstmt1_.fn_,pp.pstmt1_.l_,
				pp.pstmt2_.fn_,pp.pstmt2_.l_)
			lines.append(s)
		outfile.writelines(lines)
		outfile.close()

if __name__=='__main__':
	if len(sys.argv)!=3:
		print 'usage: group.py [infile_name] [outfile_dir]'
	else:
		infile_name=sys.argv[1]
		outfile_name=sys.argv[2]
		group(infile_name)
		export(outfile_name)
