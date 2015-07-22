#!/usr/bin/env python

"""
parse the output file from LOCKSMITH
"""

import sys

file_and_line=[]

def handleWarnings(infile):
	while True:
		lines=infile.readlines(10000)
		if not lines:
			break
		has_warning=False
		has_dereference=False

		for line in lines:
			if not has_warning and line.find('Warning: Possible data race')==0:
				has_warning=True;continue
			if not has_dereference and has_warning and line.find('dereference')!=-1:
				has_dereference=True
			if has_dereference and line.find('locks acquired')!=-1:
				has_dereference=False;continue
			if has_dereference:
				handleLine(line)

def handleLine(line):
	"""
	find file and line info correlative to shared location
	"""
	items=line.split(' ')
	for item in items:
		if item.find(':')!=-1:
			subitems=item.split(':')
			file_and_line.append(subitems[-2]+'\t'+subitems[-1].strip()+'\n')

def parse():
	"""
	do the parse work
	"""
	if len(sys.argv)!=3:
		print 'usage: parse [infile_name] [outfile_name]'
		return
	infile_name=sys.argv[1]
	outfile_name=sys.argv[2]

	infile=open(infile_name)
	handleWarnings(infile)
	outfile=open(outfile_name,'w')
	outfile.writelines(set(file_and_line))
	infile.close()
	outfile.close()

if __name__=='__main__':
	parse()




	
