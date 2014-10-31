#include <ctime>
#include <cstdlib>
#include <assert.h>
#include <iostream>
#include <set>
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::set;
using std::auto_ptr;
#include "poker.hpp"

template<typename C, typename T>
bool contains(C c, T t)
{
  return c.find(t) != c.end();
}

// TODO: use better rand method
static void select_cards(poker_card_t cards[], int size, set<int>& used_cards)
{
  for(int i = 0; i < size; ++i) {
    int propose = rand() % 52 ;
    while(contains(used_cards, propose)) {
      propose = rand() % 52 ;
    }
    cards[i] = poker_card_t(propose);
    used_cards.insert(propose);
    cout << "\t" << i << ": " << cards[i] << endl;
  }
}

template<typename T>
static void select_cards(T cards, int round, int players, set<int>& used_cards)
{
  for(int i = 0; i < players; ++i) {
    int propose = rand() % 52 ;
    while(contains(used_cards, propose)) {
      propose = rand() % 52 ;
    }
    cards[i][round] = poker_card_t(propose);
    used_cards.insert(propose);
    cout << "\t" << i << ": " << cards[i][round] << endl;
  }
}

std::ostream& operator<<(std::ostream& os, const poker_card_t& card)
{
  cout << "(";
  switch(card.m_num / 13) {
    case 0:
      cout << "spade";
      break;
    case 1:
      cout << "heart";
      break;
    case 2:
      cout << "diamond";
      break;
    case 3: 
      cout << "club";
      break;
    default:
      assert(false);
  } 
  cout << " " << (card.m_num % 13) + 1 << ")";
  return os;
}

poker_game_t::poker_game_t(int players)
  : m_players(players) 
{
}

void poker_game_t::start()
{
  while(deal()) {
    ;
  }
  output_cards();
}

blackjack_t::~blackjack_t()
{
  delete[] m_cards;
}

blackjack_t::blackjack_t(int players)
  : poker_game_t(players),
    round(0)
{
  m_cards = new poker_card_t[m_players][2];
}

bool blackjack_t::can_play()
{
  return m_players <= 52/2;
}

bool blackjack_t::deal()
{
  cout << "Round " << round << ":" << endl;
  select_cards(m_cards, round, m_players, used_cards);
  return ++round < 2; 
}

void blackjack_t::output_cards()
{
  cout << "Players:" << endl;
  for(int i = 0; i < m_players; ++i) {
    cout << i << ": " << endl;
    cout << "\t" << m_cards[i][0] << ", " << m_cards[i][1] << endl;
  }
}

poker_stud_t::~poker_stud_t()
{
  delete[] m_cards;
}

poker_stud_t::poker_stud_t(int players)
  : poker_game_t(players),
    round(0)
{
  m_cards = new poker_card_t[m_players][7]; 
}

bool poker_stud_t::can_play()
{
  return m_players <= 52/7;
}

bool poker_stud_t::deal()
{
  cout << "Round " << round << ":" << endl;
  select_cards(m_cards, round, m_players, used_cards);
  return ++round < 7; 
}

void poker_stud_t::output_cards()
{
  cout << "Players:" << endl;
  for(int i = 0; i < m_players; ++i) {
    cout << i << ": " << endl;
    cout << "\t" ;
    for(int j = 0; j < 7; ++j) {
      cout << m_cards[i][j] << ", ";
    }
    cout << endl;
  }
}

poker_texas_t::~poker_texas_t()
{
  delete[] m_cards;
}

poker_texas_t::poker_texas_t(int players)
  : poker_game_t(players),
    round(0)
{
  m_cards = new poker_card_t[m_players][2];
}

bool poker_texas_t::can_play()
{
  return m_players <= (52 -5)/2;
}

bool poker_texas_t::deal()
{
  if(round == 0) {
    cout << "Community: " << endl;
    select_cards(m_community_cards, 5, used_cards);
  }
  cout << "Round " << round << ":" << endl;
  select_cards(m_cards, round, m_players, used_cards);
  return ++round < 2; 
}

void poker_texas_t::output_cards()
{
  cout << "Community:" << endl;
  cout << "\t";
  for(int i = 0; i < 5; ++i) {
    cout << m_community_cards[i] << ", ";
  }
  cout << endl;
  cout << "Players:" << endl;
  for(int i = 0; i < m_players; ++i) {
    cout << i << ": " << endl;
    cout << "\t" << m_cards[i][0] << ", " << m_cards[i][1] << endl;
  }
}

string poker_game_console_t::parse_string(game_t ngame)
{
  switch(ngame) {
    case G_BLACKJACK:
      return "blackjack";
    case G_POKER_STUD:
      return "7-card stud";
    case G_POKER_TEXAS:
      return "texas hold'em";
  }
  assert(false);
}

void poker_game_console_t::run()
{
  auto_ptr<poker_game_t> game;

  while(true) {
    // enter players
    int players = 0;
    do {
      cout << "Please enter the number of players: ";
      cin >> players;
      cout << endl;
      if(!cin.good()) {
        return;
      }
    } while(players <= 0);

    // select game
    int num = 0;
    do {
      cout << "Please select the game:" << endl;
      cout << (int)G_BLACKJACK << " " << parse_string(G_BLACKJACK) << endl;
      cout << (int)G_POKER_STUD << " " << parse_string(G_POKER_STUD) << endl;
      cout << (int)G_POKER_TEXAS << " " << parse_string(G_POKER_TEXAS) << endl;
      cout << "Enter: ";
      cin >> num;
      cout << endl;
      if(!cin.good()) {
        return;
      }
    } while (num < (int)G_BLACKJACK || num > (int)G_POKER_TEXAS);
    // safely converted to enum game_t
    m_ngame = (game_t)num;

    // check game and players
    switch(m_ngame) {
      case G_BLACKJACK:
        game.reset(new blackjack_t(players));
        break;
      case G_POKER_STUD:
        game.reset(new poker_stud_t(players));
        break;
      case G_POKER_TEXAS:
        game.reset(new poker_texas_t(players));
        break;
      default:
        assert(false);
    }
    
    if(!game->can_play()) {
      cout << "The number of players is too much for the game." << endl;
      continue;
    }

    game->start();
  }
}

int main(int argc, const char** argv) 
{
  srand(time(NULL));
  cout << "****** Welcome to POKER! ******* " << endl;
  cout << "Enter ctrl-D to exit. " << endl;
  poker_game_console_t().run();
}
