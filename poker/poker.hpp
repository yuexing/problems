#ifndef POKER_HPP
#define POKER_HPP 

#include <string>
#include <iostream>
#include <set>
#include <memory>

struct poker_card_t
{
public:
  friend std::ostream& operator<<(std::ostream& os, const poker_card_t& card);

  poker_card_t()
    : m_num(-1) {}
  poker_card_t(int num)
    : m_num(num) {}
private:
  int m_num;
};

std::ostream& operator<<(std::ostream& os, const poker_card_t& card);

// 4 suites of 13 cards
class poker_game_t
{
public:
  poker_game_t(int players);
  virtual ~poker_game_t() {}

  void start();
  virtual bool can_play() = 0;

protected:
  // deal one at a time to each player.
  // return true means more deals are needed; false otherwise.
  virtual bool deal() = 0;

  // 
  virtual void output_cards() = 0;

  int m_players;
  std::set<int> used_cards;
};

// 2 cards each player
class blackjack_t : public poker_game_t
{
public:
  blackjack_t(int players);
protected:
  bool deal();
  bool can_play();
  void output_cards();

  int round;
  poker_card_t (*m_cards)[2];
};

// 7 cards each player
class poker_stud_t : public poker_game_t
{
public:
  poker_stud_t(int players);
protected:
  bool deal();
  bool can_play();
  void output_cards();

  int round;
  poker_card_t (*m_cards)[7];
};

// 2 cards each player, 5 community cards 
class poker_texas_t : public poker_game_t
{
public:
    poker_texas_t(int players);
protected:
  bool deal();
  bool can_play();
  void output_cards();
  
  int round;
  poker_card_t (*m_cards)[2];
  poker_card_t m_community_cards[5];
};

class poker_game_console_t
{
public:
  void run();
private:
  enum game_t {
    G_BLACKJACK = 1,
    G_POKER_STUD,
    G_POKER_TEXAS
  };

  // return the human-readable string for the game
  std::string parse_string(game_t ngame);

  game_t m_ngame; 
};
#endif /* POKER_HPP */
