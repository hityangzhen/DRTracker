#!/usr/bin/env python

import sys
import os
help='usage:converter.py [infile_name] [outfile_name]'

def convert(infile_name,outfile_name):
	"""
	eliminate the `#line` macro
	"""
	infile=open(infile_name)
	outfile=open(outfile_name,'w')
	while True:
		lines=infile.readlines(500)
		outlines=[]
		if not lines:
			break
		for line in lines:
			if line.startswith('#line'):
				outlines.append('\n')
			else:
				outlines.append(line)
		outfile.writelines(outlines)
	infile.close()
	outfile.close()

def convert_all(dir,outfile_pattern):
	"""
	"""
	if os.path.isdir(dir):
		files=os.listdir(dir)
		for file in files:
			if os.path.isfile(file) and (file.endswith('.c') or 
				file.endswith('.h')):
				if file.endswith('.c'):
					outfile_name=file.replace('.c',outfile_pattern+'.c')
				else:
					outfile_name=file.replace('.h',outfile_pattern+'.h')
				convert(file,outfile_name)

if __name__=='__main__':
	if len(sys.argv)!=4:
		print help
	else:
		if sys.argv[1]=='single':
			convert(sys.argv[2],sys.argv[3])
		elif sys.argv[1]=='all':
			convert_all(sys.argv[2],sys.argv[3])
