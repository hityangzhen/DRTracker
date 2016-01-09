#!/usr/bin/env python

"""
verify the data race in multigroups
"""

import os
import sys

tool_path='/home/yiranyaoqiu/RaceChecker/'

cmd="/home/yiranyaoqiu/pin/pin -t %sbuild-debug/race_profiler.so \
-partial_instrument 1 -race_verify_ml 1 -history_race_analysis 0 \
-static_profile %s%s -instrumented_lines %s \
-ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 \
-debug_main 1 -debug_pthread 1 -debug_malloc 1 -- %s "

sinfo_name=tool_path+'sinfo.db'
race_name=tool_path+'race.rp'
race_db_name=tool_path+'race.db'

help="useage: group_verifier.py [group_dir] \
	[instrumented_lines filename] [executable filename]"

class RStmt(object):
	"""
	racy stmt
	"""
	def __init__(self,fn,l):
		self.fn_=fn
		self.l_=l

class RStmtPair(object):
	"""
	racy stmt pair
	"""
	def __init__(self,rstmt1,rstmt2):
		self.rstmt1_=rstmt1
		self.rstmt2_=rstmt2

race_info=[]

def remove_race_files():
	"""
	remove older static info, race database file and race report file
	"""
	if os.path.exists(sinfo_name):
		os.remove(sinfo_name)
	if os.path.exists(race_name):
		os.remove(race_name)
	if os.path.exists(race_db_name):
		os.remove(race_db_name)

def load_race_info():
	"""
	load the race info and remove the relevant unuseful files
	"""
	race_file=open(race_name)
	print 'load race info'
	while True:
		lines=race_file.readlines(100)
		if not lines:
			break
		for line in lines:
			race_info.append(line)
	race_file.close()
	# remove static info file and race database file
	remove_race_files()

def group_verifier(group_dir,instr_name,exec_name):
	# remove static info file and race database file
	remove_race_files()
	
	group_dir=tool_path+group_dir
	instr_name=tool_path+instr_name
	exec_name=tool_path+exec_name

	if os.path.isdir(group_dir) and os.path.isfile(instr_name) \
		and os.path.isfile(exec_name):
		# traverse the group files in the directory
		for parent,dirnames,filenames in os.walk(group_dir):
			for filename in filenames:
				if filename.startswith('g') and filename.endswith('.out'):
					# exec the shell command
					if not group_dir.endswith('/'):
						group_dir=group_dir+'/'
					sh=cmd % (tool_path,group_dir,filename,instr_name,exec_name)
					print sh
					output=os.popen(sh)
					# read the output
					while True:
						lines=output.readlines(100)
						if not lines:
							break
						for line in lines:
							print line
					output.close()
					load_race_info()
					print 'success'
			# export to the race file
			race_file=open(race_name,'w')
			race_file.writelines(race_info)
			race_file.close()
	else:
		print help

if __name__=='__main__':
	if len(sys.argv)!=4:
		print help
	else:
		group_verifier(sys.argv[1],sys.argv[2],sys.argv[3])