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

#ifndef POSITION_HPP
#define POSITION_HPP

#include <string>
#include <cstdint>
#include <iostream>

namespace GameSolver { namespace Connect4 {

  // 보드의 맨 아랫줄을 1로 다 채운 것.
  constexpr static uint64_t bottom(int width, int height) {
    return width == 0 ? 0 : bottom(width-1, height) | 1LL << (width-1)*(height+1);
  }

  class Position {
    public:

      // 보드 및 알파 베타 프루닝을 위한 기본 상수
      // 스코어 계산법에 대한 자세한 내용은 보고서 참조.
      static const int WIDTH = 7;
      static const int HEIGHT = 6;
      static const int MIN_SCORE = -(WIDTH*HEIGHT)/2 + 3;
      static const int MAX_SCORE = (WIDTH*HEIGHT+1)/2 - 3;
      static_assert(WIDTH < 10, "Board's width must be less than 10");
      static_assert(WIDTH*(HEIGHT+1) <= 64, "Board does not fit in 64bits bitboard");

      // 최상위 컬럼의 비트와 마스크(지금까지 둔 곳)의 &연산을 했을 때 1이라면 최상위 컬럼이 채워진 것 => 진행 불가.
      // 0 이라면 아직 채워지지 않았으므로 진행 가능.
      // 단, 게임에서 자유롭게 쓸 수 있게 public으로 변경
      bool canPlay(int col) const
      {
        return (mask & top_mask_col(col)) == 0;
      }

      // 실제로 착수한다. (moves 1 증가)
      // current_position 에는 착수된 곳 중 O/X 가 번갈아서 1/0이 된다.
      // mask는 O/X 구분하지 않고 착수된 곳이 모두 1이 된다.
      // void play(int col)
      // {
      //   current_position ^= mask;
      //   mask |= mask + bottom_mask_col(col);
      //   moves++;
      // }
      void play(uint64_t move)
      {
        current_position ^= mask;
        mask |= move;
        moves++;
      }

      // 실제로 착수한다. 2번째 버전.
      // 컬럼 기반의 착수와 비트 기반의 착수 2가지 방식이 있다.
      // 이 또한 오리지널 코드와 다르게 퍼블릭으로 게임에서 호출하게끔 변경
      void playCol(int col)
      {
        play((mask + bottom_mask_col(col)) & column_mask(col));
      }

      // 오리지널 코드에서 불필요한 코드(벤치마크용)는 삭제하였다.

      // 다음 착수로 승리할 수 있는지 체크한다.
      bool canWinNext() const
      {
        return winning_position() & possible();
      }

      // 지금까지 몇 개의 돌을 착수했는지 리턴한다.
      int nbMoves() const
      {
        return moves;
      }

      // 현재 포지션을 대표하는 독립적인 키를 리턴한다.
      // 캐쉬 작업에 사용된다.
      // key = current_position + mask
      uint64_t key() const
      {
        return current_position + mask;
      }

      // 해당 컬럼을 착수했을 때 승리하는 지 여부를 결과로 리턴
      // 다른 곳에서도 쓸 수 있게끔 public으로 구현.
      //   해당 컬럼에
      //    착수할 수 있는 수가 존재하고,
      //     그 수가 승리하는 수라면,
      // 해당 컬럼에 착수했을 때 바로 승리할 수 있다.
      bool isWinningMove(int col) const
      {
        return winning_position() & possible() & column_mask(col);
      }

      // 해당 컬럼에 착수했을 때 다음 상대방의 착수로 인해 패배하지 않는 경우 등을 고려하여,
      // 지금 당장 착수해도 아무런 문제가 없는, 즉 지지 않는 착수들을 모두 고려하여 1로 마킹한
      // 비트를 리턴한다.
      // 수를 예측하여 프루닝을 향상시키는 데 사용되며 성능을 비약적으로 향상시킨다.
      uint64_t possibleNonLosingMoves() const {
        assert(!canWinNext());
        // 착수 가능한 곳 마킹
        uint64_t possible_mask = possible();
        // 상대방이 착수 시 이기는 곳 마킹
        uint64_t opponent_win = opponent_winning_position();
        // 내가 둘 수 있는 데 두지 않을 경우, 상대가 두어서 승리하므로, 해당 위치는 강제하여 착수한다.
        uint64_t forced_moves = possible_mask & opponent_win;

        // 그러한 강제 착수 위치가 존재한다면,
        if(forced_moves) {
          if(forced_moves & (forced_moves - 1)) // check if there is more than one forced move
            return 0;                           // the opponnent has two winning moves and you cannot stop him
          else possible_mask = forced_moves;    // enforce to play the single forced move
        }
        // 상대방이 바로 이길 수 있는 수의 발판이 되지 않게끔 착수한다.
        return possible_mask & ~(opponent_win >> 1);  // avoid to play below an opponent winning spot
      }

      // 승리가능한 포인트를 마킹하여 카운트한 것을 moveScore라는 점수로 계산한다.
      // 승리 포인트가 많다는 것은 그만큼 이길 확률이 높다는 의미와 같다.ㅇ
      int moveScore(uint64_t move) const {
        return popcount(compute_winning_position(current_position | move, mask));
      }

      // 현재 포지션 기준 상대방 입장에서 승리 가능한 포지션을 비트로 리턴.
      uint64_t opponent_winning_position() const {
        return compute_winning_position(current_position ^ mask, mask);
      }

      // 현재까지 착수된 수를 의미하는 mask 기반으로 바텀마스크를 더한 뒤, 보드 마스크와 & 연산을 하면,
      // 착수가 가능한 부분만 1로 만든 비트가 생긴다. (컬럼이 꽉차서 넘어간 경우 보드마스크로 짤리게 되어 착수 불가)
      // mask     +   bottom_mask
      // 0000000   // 0000000
      // 0000000   // 0000000
      // 0000000   // 0010000
      // 0010000   // 0000000
      // 0010000   // 0000101
      // 0010101   // 0100010
      // 0110111   // 1001000
      uint64_t possible() const {
        return (mask + bottom_mask) & board_mask;
      }

      // 생성자.
      // 오리지널 코드에서 수행하던 불필요한 초기화 연산은 삭제하고 상수로 초기화 하였다.
      Position() : current_position{0}, mask{0}, moves{0} {}

    private:
      // 착수와 관련된 변수
      // 비트맵으로 선공/후공을 구분하고,
      // 돌이 놓여져 있는 곳을 마스킹한다.
      // 그리고 현재까지 총 몇수가 진행되었는지도 관리한다.
      uint64_t current_position;
      uint64_t mask;
      unsigned int moves;

      // 현재 O/X가 누구인지, 어디까지 착수되었는지를 기반으로 승리 가능한 포지션을 비트로 리턴.
      uint64_t winning_position() const {
        return compute_winning_position(current_position, mask);
      }

      // 주어진 비트 속에서 1을 전부 센다.
      static unsigned int popcount(uint64_t m) {
        unsigned int c = 0;
        for (c = 0; m; c++) m &= m - 1;
        return c;
      }

      // 현재 착수하는 사람의 포지션과 마스크를 기반으로, 다음 착수 중 승리하게 되는 위치를 계산한다.
      static uint64_t compute_winning_position(uint64_t position, uint64_t mask) {
        // 수직 연속 3개 여부 확인
        // 수직 연속 3개라면 쉬프트를 1, 2, 3한 뒤 & 연산 시 겹치는 곳이 나온다.
        // 0 0 0
        // 0 0 0
        // 0 0 1
        // 0 1 1
        // 1 1 1 <- r
        // 1 1 0
        // 1 0 0
        uint64_t r = (position << 1) & (position << 2) & (position << 3);

        // 수평 연속 3개 혹은 1개-빈칸-2개 여부 확인
        // 0 1 1 1 0
        //   0 1 1 1 0
        //     0 1 1 1 0
        //         | 여기
        // 또는
        // 0 1 1   1 0
        //   0 1 1   1 0
        //     0 1 1   1 0
        //       | 여기

        uint64_t p = (position << (HEIGHT+1)) & (position << 2*(HEIGHT+1));
        r |= p & (position << 3*(HEIGHT+1));
        r |= p & (position >> (HEIGHT+1));
        p = (position >> (HEIGHT+1)) & (position >> 2*(HEIGHT+1));
        r |= p & (position << (HEIGHT+1));
        r |= p & (position >> 3*(HEIGHT+1));

        // 대각선 1 (역슬래쉬 방향) : 6칸 쉬프트를 하면 우하향으로 1칸씩 이동하게 된다.
        p = (position << HEIGHT) & (position << 2*HEIGHT);
        r |= p & (position << 3*HEIGHT);
        r |= p & (position >> HEIGHT);
        p = (position >> HEIGHT) & (position >> 2*HEIGHT);
        r |= p & (position << HEIGHT);
        r |= p & (position >> 3*HEIGHT);

        // 대각선 2 (슬래쉬 방향 /) : 6칸 역쉬프트를 하면 좌상향으로 1칸씩 이동하게 된다.
        p = (position << (HEIGHT+2)) & (position << 2*(HEIGHT+2));
        r |= p & (position << 3*(HEIGHT+2));
        r |= p & (position >> (HEIGHT+2));
        p = (position >> (HEIGHT+2)) & (position >> 2*(HEIGHT+2));
        r |= p & (position << (HEIGHT+2));
        r |= p & (position >> 3*(HEIGHT+2));

        // board_mask ^ mask 는 지금까지 두지 않은 곳을 1로 마킹한 것을 의미한다.
        // r은 착수 시 바로 승리가능한 곳을 의미한다.
        // 즉, r & (board_mask ^ mask)는 착수 가능하고, 바로 승리가 가능한 곳을 의미한다.
        return r & (board_mask ^ mask);
      }


      // 맨 아랫줄을 1로 다 채운 것 & 보드를 전부 1로 채운 것.
      // ex) bottom_mask = bottom(7,6);
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      // 1111111
      // ex) board_mask
      // 0000000
      // 1111111
      // 1111111
      // 1111111
      // 1111111
      // 1111111
      // 1111111
      const static uint64_t bottom_mask = bottom(WIDTH, HEIGHT);

      // 보드 마스크(7번째 줄을 제외한 실제 게임이 이뤄지는 보드만 마스킹한 것)
      const static uint64_t board_mask = bottom_mask * ((1LL << HEIGHT)-1);

      // col 에 해당하는 컬럼의 맨 위에 1 한개를 채운 것.
      // ex) top_mask_col(1)
      // 0000000
      // 0100000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      static constexpr uint64_t top_mask_col(int col) {
        return UINT64_C(1) << ((HEIGHT - 1) + col*(HEIGHT+1));
      }

      // col 에 해당하는 컬럼의 맨 아래에 1 한개를 채운 것.
      // ex) bottom_mask_col(1)
      // 0000000
      // 0100000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      // 0000000
      static constexpr uint64_t bottom_mask_col(int col) {
        return UINT64_C(1) << col*(HEIGHT+1);
      }

    public:
      // col 에 해당하는 컬럼을 1로 채운 것.
      // ex) column_mask(1)
      // 0000000
      // 0100000
      // 0100000
      // 0100000
      // 0100000
      // 0100000
      // 0100000
      static constexpr uint64_t column_mask(int col) {
        return ((UINT64_C(1) << HEIGHT)-1) << col*(HEIGHT+1);
      }

  };

}} // end namespaces

#endif
