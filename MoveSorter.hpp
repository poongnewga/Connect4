/*
 * This file is part of Connect4 Game Solver <http://connect4.gamesolver.org>
 * Copyright (C) 2007 Pascal Pons <contact@gamesolver.org>
 *
 * Connect4 Game Solver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Connect4 Game Solver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Connect4 Game Solver. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * [2018 인공지능 : 선배들을 이겨라!]
 *   Destroy AI - 채희재, 이태훈, 문선미
 *   >> Connect4 Game Solver 메인 로직 커스터마이징, 게임 구현 및 스타일링, 6번 수 이후 룰 - 채희재
 *   >> 5번 수까지의 룰, 테스팅, QA - 이태훈, 문선미
 * 본 코드는 위 주석에서 언급되었듯이
 *   공개코드인 Connect4 Game Solver <http://connect4.gamesolver.org> 를 기반으로 합니다.
 * 본 저작권자의 요구에 따라 GNU Affero GPL 을 따라 <https://github.com/poongnewga/Connect4>에 코드가 모두 공개되어 있습니다.
 * 따라서 본 코드 또한 GNU Affero GPL을 따릅니다.
 * 자세한 내용은 GNU Affero General Public License <http://www.gnu.org/licenses/> 참조.
 */

#ifndef MOVE_SORTER_HPP
#define MOVE_SORTER_HPP

#include "position.hpp"

namespace GameSolver { namespace Connect4 {
  class MoveSorter {
  public:

    void add(uint64_t move, int score)
    {
      int pos = size++;
      for(; pos && entries[pos-1].score > score; --pos) entries[pos] = entries[pos-1];
      entries[pos].move = move;
      entries[pos].score = score;
    }

    /*
     * Get next move
     * @return next remaining move with max score and remove it from the container.
     * If no more move is available return 0
     */
    uint64_t getNext()
    {
      if(size)
        return entries[--size].move;
      else
        return 0;
    }

    /*
     * reset (empty) the container
     */
    void reset()
    {
      size = 0;
    }

    /*
     * Build an empty container
     */
    MoveSorter(): size{0}
    {
    }

  private:
    // number of stored moves
    unsigned int size;

    // Contains size moves with their score ordered by score
    struct {uint64_t move; int score;} entries[Position::WIDTH];
  };

}}; // namespaces

#endif
