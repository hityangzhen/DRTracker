#!/usr/bin/env python

"""
parse the output file from RELAY
(Warning.xml and Warning2.xml)
not well formatted
***********************************************************************
<?xml version="1.0"?>
<run>
	<cluster id="7">
	 	<race>
	 		<acc1>
				<lval printed="[REP: 16].x" size="1"></lval>
	  			<pp file="threads.c" line="111" parent="30.f"></pp>
	 			<locks rep="false"></locks>
	  			<cp root="30.f">
	   				<spawned_at> <pp file="threads.c" line="295" ></pp>
	   				</spawned_at>
	   			</cp>
	 		</acc1>
	 		<acc2>
	 			<lval printed="[REP: 16].x" size="1"></lval>
	  			<pp file="threads.c" line="111" parent="30.f"></pp>
	 			<locks rep="false"></locks>
	  			<cp root="30.f">
	   				<spawned_at> <pp file="threads.c" line="295" ></pp>
	   				</spawned_at>
	   			</cp>
	 		</acc2>
	 	</race>
	</cluster>
</run>
</xml>
***********************************************************************
"""

import xml.dom.minidom
import sys
import os
from tool import file_and_line_sort

file_and_line=[]

def handleWarningLines(infile_name):
	"""all the warning lines are accumulated into file_and_line
	list, each entry is a string of 
		`file,SPACE and line`
	this method is mainly used for accumulating potential racing
	statements of the evaluated program
	"""
	dom=xml.dom.minidom.parse(infile_name)
	root=dom.documentElement
	race_nodes=root.getElementsByTagName('race')
	for race_node in race_nodes:
		race_acc1=race_node.getElementsByTagName('acc1')[0]
		# find the pp child nodes
		for node in race_acc1.childNodes:
			if node.nodeType==node.ELEMENT_NODE and node.nodeName=='pp':
				file_and_line.append(node.getAttribute('file')+' '+
					node.getAttribute('line')+'\n')

		race_acc2=race_node.getElementsByTagName('acc2')[0]

		for node in race_acc2.childNodes:
			if node.nodeType==node.ELEMENT_NODE and node.nodeName=='pp':
				file_and_line.append(node.getAttribute('file')+' '+
					node.getAttribute('line')+'\n')

def handleWarningPairs(infile_name):
	"""all the warning pairs are accumulated into file_and_line
	list, each entry is a string of 
		`file,SPACE,line,SPACE,file,SPACE,line`
	this method is mainly used for accumulating potential racing 
	statement pairs of the evaluated program
	"""
	dom=xml.dom.minidom.parse(infile_name)
	root=dom.documentElement
	race_nodes=root.getElementsByTagName('race')
	for race_node in race_nodes:
		race_acc1=race_node.getElementsByTagName('acc1')[0]
		# find the first potential racing stmts of pairs
		first_stmts=[]
		for node in race_acc1.childNodes:
			if node.nodeType==node.ELEMENT_NODE and node.nodeName=='pp':
				first_stmts.append(node.getAttribute('file')+' '+
					node.getAttribute('line'))

		race_acc2=race_node.getElementsByTagName('acc2')[0]
		# find the second potential racing stmts of pairs
		second_stmts=[]
		for node in race_acc2.childNodes:
			if node.nodeType==node.ELEMENT_NODE and node.nodeName=='pp':
				second_stmts.append(node.getAttribute('file')+' '+
					node.getAttribute('line'))
		# merge into the pair
		for first_stmt in first_stmts:
			for second_stmt in second_stmts:
				file_and_line.append(first_stmt+' '+second_stmt+'\n')

def removeUnformattedLine(infile_name):
	infile=open(infile_name)
	tmp_file=open(infile_name+'.tmp','w')
	while True:
		lines=infile.readlines(10000)
		if not lines:
			break
		elif not lines[-1].startswith('</xml>'):
			tmp_file.writelines(lines)
			continue
		# unformatted line
		tmp_file.writelines(lines[:-1])
	tmp_file.close()
	infile.close()

def parse():
	"""
	do the parse work
	"""
	if len(sys.argv)!=3:
		print 'usage: parse [infile_dir] [outfile_name]'
		return

	infile_name1=os.path.join(sys.argv[1],'warnings.xml')
	infile_name2=os.path.join(sys.argv[1],'warnings2.xml')
	outfile_name=sys.argv[2]

	removeUnformattedLine(infile_name1)
	removeUnformattedLine(infile_name2)

	# we should differentiate the output type is instrumented_lines
	# file or static_profile file

	if outfile_name.startswith('instrumented_lines'):
		handleWarningLines(infile_name1+'.tmp')
		handleWarningLines(infile_name2+'.tmp')
	elif outfile_name.startswith('static_profile'):
		handleWarningPairs(infile_name1+'.tmp')
		handleWarningPairs(infile_name2+'.tmp')

	outfile=open(outfile_name,'w')
	if outfile_name.startswith('instrumented_lines'):
		outfile.writelines(file_and_line_sort(list(set(file_and_line)),False))
	elif outfile_name.startswith('static_profile'):
		outfile.writelines(file_and_line_sort(list(set(file_and_line)),True))
	outfile.close()

	# remove the original file
	os.remove(infile_name1+'.tmp')
	os.remove(infile_name2+'.tmp')

if __name__=='__main__':
	parse()











