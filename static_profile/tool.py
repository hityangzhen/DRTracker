def file_and_line_sort(file_and_line):
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

	return sorted(file_and_line,alpha_numeric_compare)