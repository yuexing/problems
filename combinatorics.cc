/**
 * The wiki page: http://en.wikipedia.org/wiki/Combinatorics 
 * has listed a lot of interesting stuff to implement. 
 * Implement them in a way of 'yield-return'.
 */

// "0ab" -> ["0ab", "0aB", "0Ab", "0AB"])


// minCoin

// Write a program to implement Boggle Game.
// You are given a 4x4 matrix of letters and a dictionary, find all the valid
// words in the matrix. Following are the conditions
// 1. If a letter is used, it should not be used again in the same word search
// 2. The word path can be of any direction
// 3. There has to be a path of the letters forming the word( in other words
// all the letters in the word must have to adjacent to one another)
//
// Example:
//
//
// D A T H
// C G O A
// S A T L
// B E D G
//
//
// Some of the Valid words are:
// DATA, HALO, HALT, SAG, BEAT, TOTAL, GLOT, DAG
//
//
// Not valid words:
// DAGCD ( D cannot be used again)
// DOG ( There is no path between letters)

// Jump Game:
// Given an array start from the first element and reach the last by jumping.
// The jump length can be at most the value at the current position in the
// array. Optimum result is when u reach the goal in minimum number of jumps.
// For ex:
// Given array A = {2,3,1,1,4}
// possible ways to reach the end (index list)
// i) 0,2,3,4 (jump 2 to index 2, then jump 1 to index 3 then 1 to index 4)
// ii) 0,1,4 (jump 1 to index 1, then jump 3 to index 4)
// Since second solution has only 2 jumps it is the optimum result.
