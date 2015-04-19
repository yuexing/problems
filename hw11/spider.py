import random
import json
import requests
import bs4
from collections import OrderedDict
from operator import itemgetter

class Repo:
	def __init__(self, name, info, lang, stars, forks):
		self.name = name
		self.info = info
		self.lang = lang
		self.stars = stars
		self.forks = forks

def grab(company, pages):
	f = open('./' + company + ".json", 'w+')
	print("{\"repos\": [", file=f)

	isFirst = True

	for i in range(1, pages):
		url = "https://github.com/" + company
		if(i > 1) :
			url += "?page="+str(i)

		response = requests.get(url);
		soup = bs4.BeautifulSoup(response.text)
		repos = soup.select('div.repo-list-item');

		for repo in repos:
			stats = repo.select('div.repo-list-stats')[0]
			lang = stats.select('span')[0].get_text().strip();

			texts = [a.get_text().strip() for a in stats.select('a')]
			stars = texts[0]
			forks = texts[1]

			name = repo.select('.repo-list-name')[0].select('a')[0].get_text().strip();
			info = ""
			if(repo.select('.repo-list-description')):
				info = repo.select('.repo-list-description')[0].get_text().strip();

			repo = Repo(name, info, lang, stars, forks)
			if(not isFirst):
				print(",", file=f)	#trailing comma

			print(json.dumps(repo.__dict__), end="", file=f)
			isFirst = False

	print("]}" , file=f)
	f.close()


#grab("google", 23);
#grab("linkedin", 4);
#grab("twitter", 7);
#grab("facebook", 6);
#grab("square", 7);

"""
langs = {}
def record_lang(company):
	with open(company + ".json") as data_file:    
		data = json.load(data_file)
    	for repo in data['repos']:
    		lang = repo['lang']

    		if(lang not in langs):
    			langs[lang] = 0

    		langs[lang] = langs[lang] + 1

record_lang("google");
record_lang("linkedin");
record_lang("twitter");
record_lang("facebook");

for key in langs:
	print key, ": ", langs[key]

d = OrderedDict(sorted(langs.items(), key=itemgetter(1), reverse=True))
print d
"""

known_lang = set(['Java', 'Python', 'Javascript', 'C/C++', 'Scala', 'Dart', 'Go', 'Others']);

class mylist(list):
	def __str__(self):
		return json.dumps(self)

def process(company):
	f = open('./' + company + "_data.json", 'w+')

	repos = {}
	with open(company + ".json") as data_file:    
		data = json.load(data_file)
		repos = data['repos']

	# time: 2010 -> 2015 (5)
	# total: 1 -> 5 * 10
	# permonth: 0 -> 10
	total = random.randrange(1, 51)
	data = {}
	for i in range(1, total):
		time = 2010 + (random.uniform(0, 1) * 5)
		permonth = random.randrange(0, 11)
		commits = mylist()
		for i in range(1, permonth):
			idx = random.randrange(0, len(repos))
			if(repos[idx]['lang'] not in known_lang):
				repos[idx]['lang'] = 'Others'

			commits.append(repos[idx])

		data["{0:.1f}".format(time)] = commits

	isFirst = True

	print("{\"name\": \"" + company + "\",", file=f)
	print("\"commits\": [", file=f)
	for key in data:
		if(not data[key]):
			continue

		if(not isFirst):
			print(",", file=f)

		print("{\"time\": " + key + ",", file=f)
		print("\"commits\": ", end="", file=f) 
		print(data[key], end="", file=f)
		print("}", end="", file=f)

		isFirst = False

	print("]}", file=f)
	f.close()

#process("google");
#process("linkedin");
#process("twitter");
#process("facebook");
#process("square")