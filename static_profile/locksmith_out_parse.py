#!/usr/bin/env python

"""
parse the output file from LOCKSMITH
"""

import sys
from tool import file_and_line_sort
# import os

file_and_line=[]
files=[]

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
			if subitems[-1].strip()!='-1':
				p=subitems[-2].rfind('/')
				file_and_line.append(subitems[-2][p+1:]+' '+subitems[-1].strip()+'\n')

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

	outfile.writelines(file_and_line_sort(list(set(file_and_line))))
	
	infile.close()
	outfile.close()

# def walk_dir(dir):
# 	"""
# 	get all *.c file in current dir and it's children dirs
# 	"""
# 	l=[]
# 	for root, dirs, files in os.walk(dir):
# 		for name in files:
# 			l.append(os.path.join(root,name))
# 	files=[f for f in l if f.endswith('.c')]

if __name__=='__main__':
	parse()




	
