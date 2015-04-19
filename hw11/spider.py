import requests
import bs4
from sets import Set

def grab(company):
	print "\n=============="
	print company, ":"
	response = requests.get("https://github.com/" + company);
	soup = bs4.BeautifulSoup(response.text)
	repos = soup.select('div.repo-list-item');

	known_lang = Set(['Javascript', 'Python', 'Ruby', 'Java', 'C/C++', 'PHP', 'Objective-C', 'Go', 'Others'])

	for repo in repos:
		stats = repo.select('div.repo-list-stats')[0]
		lang = stats.select('span')[0].get_text().strip();
		if(lang not in known_lang) :
			lang = 'Others'

		texts = [a.get_text().strip() for a in stats.select('a')]
		stars = texts[0]
		forks = texts[1]

		name = repo.select('.repo-list-name')[0].select('a')[0].get_text().strip();
		info = ""
		if(repo.select('.repo-list-description')):
			info = repo.select('.repo-list-description')[0].get_text().strip();

		print "{"
		print "\"name\": \"", name, "\","
		print "\"info\": \"", info, "\","
		print "\"lang\": \"", lang, "\","
		print "\"stars\": \"", stars, "\","
		print "\"forks\": \"", forks, "\","
		print "}"


grab("google");
grab("linkedin");
grab("twitter");
grab("facebook");