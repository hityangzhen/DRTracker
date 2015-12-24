def file_and_line_sort(file_and_line,is_pair):
	"""
	sort the file_and_line str
	"""
	def alpha_numeric_compare(x,y):
		if x.strip()=='':
			return 1
		elif y.strip()=='':
			return -1
		items1=x.split()
		items2=y.split()
		# file name compare
		if items1[0] > items2[0]:
			return 1
		elif items1[0] < items2[0]:
			return -1
		else:
			return int(items1[1])-int(items2[1])

	def alpha_numeric_compare_2(x,y):
		if x.strip()=='':
			return 1
		elif y.strip()=='':
			return -1
		items1=x.split()
		items2=y.split()

		stmt1_1=items1[0]+' '+items1[1]
		stmt1_2=items1[2]+' '+items1[3]

		stmt2_1=items2[0]+' '+items2[1]
		stmt2_2=items2[2]+' '+items2[3]

		comp_1=alpha_numeric_compare(stmt1_1,stmt2_1)
		if comp_1>0:
			return 1
		elif comp_1<0:
			return -1
		else:
			return alpha_numeric_compare(stmt1_2,stmt2_2)

	if not is_pair:
		return sorted(file_and_line,alpha_numeric_compare)
	else:
		return sorted(file_and_line,alpha_numeric_compare_2)