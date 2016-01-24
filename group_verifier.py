#!/usr/bin/env python

"""
verify the data race in multigroups
"""

import os
import sys

tool_path='/home/yiranyaoqiu/RaceChecker/'
cur_path=''

cmd="/home/yiranyaoqiu/pin/pin -t %sbuild-debug/race_profiler.so \
-partial_instrument 1 -race_verify_ml 1 -history_race_analysis 0 \
-static_profile %s -instrumented_lines %s \
-ignore_lib 1 -enable_debug 0 -debug_pthread 1 -debug_mem 1 \
-debug_main 1 -debug_pthread 1 -debug_malloc 1 -- %s %s"

sinfo_name=''
race_name=''
race_db_name=''

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

def sort_filenames(filenames):
	def filename_comp(fn1,fn2):
		fn1_index=fn1.replace('.out','').replace('i','').replace('s','')
		fn2_index=fn2.replace('.out','').replace('i','').replace('s','')
		res=cmp(fn1[0],fn2[0])
		if res!=0:
			return res
		return int(fn1_index)-int(fn2_index)
	filenames.sort(filename_comp)


def group_verifier(group_dir,exec_name,args):
	# remove static info file and race database file
	remove_race_files()
	
	group_dir=os.path.join(tool_path,group_dir)
	exec_name=os.path.join(tool_path,exec_name)

	if os.path.isdir(group_dir) and os.path.isfile(exec_name):
		# traverse the group files in the directory
		for parent,dirnames,filenames in os.walk(group_dir):
			# sort
			sort_filenames(filenames)
			i_filenames=filenames[:len(filenames)/2]
			s_filenames=filenames[len(filenames)/2:]

			for i in range(len(i_filenames)):
				# exec the shell command
				s_filename=os.path.join(group_dir,s_filenames[i])
				i_filename=os.path.join(group_dir,i_filenames[i])
				sh=cmd % (tool_path,s_filename,i_filename,exec_name,args)
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
	if len(sys.argv)<3:
		print help
	else:
		cur_path=os.path.abspath('.')
		sinfo_name=os.path.join(cur_path,'sinfo.db')
		race_name=os.path.join(cur_path,'race.rp')
		race_db_name=os.path.join(cur_path,'race.db')
		# add the arguments
		args=''
		if len(sys.argv) > 3:
			for arg in sys.argv[3:]:
				args += ' '+arg
		if cur_path.find('water')!=-1:
			args = '\n'+args
		group_verifier(sys.argv[1],sys.argv[2],args)