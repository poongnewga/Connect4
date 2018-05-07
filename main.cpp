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

#include <iostream>
#include <limits>
#include <cassert>
#include "position.hpp"
#include "TranspositionTable.hpp"
#include "MoveSorter.hpp"

/*
// Connect4 Game Solver 메인 로직 커스텀 코드 by 채희재
//
// 오리지널 Connect4 Game Solver 의 지향점은 제한된 Depth 기반의 서치가 아닌 풀 서치이다.
// 2018 인공지능 수업에서 다루었던 단순 미니맥스, 알파베타 프루닝 구현에서 그치지 않고,
//
//   중앙일수록 유리하다는 휴리스틱 기반의 트리 탐색 순서 지정을 통해 프루닝 성능 향상,
//   해시 테이블 기반의 캐쉬를 두어 퍼포먼스 향상,
//   본인의 다음 수 예측을 통한 프루닝 성능 향상,
//   비트마스킹을 통한 추가 연산에 따른 추가 소요 시간 최소화
//   (1~2수를 미리 예측해 프루닝을 하는 것이 프루닝없이 추가적인 탐색을 하는 것보다 효율적)
//
// 를 통해 굉장히 오래걸리는 풀 서치를 아주 단시간내에 실현한다.
// 승리&&패배&&무승부만 체크하는 weak solver로 스코어를 계산할 경우 추가 성능 향상이 가능하나
// 수 예측을 통한 성능 향상만큼 비약적인 성능 향상이 아닌 아주 미미한 성능 향상이고,
// <http://connect4.gamesolver.org/> 에서처럼 휴리스틱을 출력하기 위해 strong solver를 사용했다.
//
*/

using namespace GameSolver::Connect4;
namespace GameSolver { namespace Connect4 {

  // 보드 7X6 단 한개만 타겟으로 개발한 코드이므로 오리지날 코드에서 불필요한 연산은 상수로 초기화
  class Solver {
    private:
    int columnOrder[Position::WIDTH] = {3, 4, 2, 5, 1, 6, 0};
    TranspositionTable transTable;

    // 현재까지 둔 수의 모음을 P라고 할 때 다음 착수를 위한 최적의 점수를 구함.
    int negamax(const Position &P, int alpha, int beta) {
      assert(alpha < beta);
      assert(!P.canWinNext());

      // 지지 않는 가능한 수를 마킹한 비트를 구한다.
      uint64_t next = P.possibleNonLosingMoves();

      // 단 1곳도 없다면, 내 착수를 발판으로 상대가 나를 이긴다.
      if(next == 0) {
        return -(Position::WIDTH*Position::HEIGHT - P.nbMoves())/2;
      }

      // 이미 모든 수 - 2 이상 뒀다면 무승부(바로 윗 조건으로 인해 42번째 수 도달 전에 무승부 여부 파악)
      if(P.nbMoves() >= Position::WIDTH*Position::HEIGHT - 2) {
        return 0;
      }

      // 단순 미니맥스, 알파베타가 아닌 수 예측 기반이므로 알파 & 베타값을 계속해서 조정
      int min = -(Position::WIDTH*Position::HEIGHT-2 - P.nbMoves())/2;
      if(alpha < min) {
        alpha = min;
        // 프루닝
        if(alpha >= beta) return alpha;
      }

      int max = (Position::WIDTH*Position::HEIGHT-1 - P.nbMoves())/2;

      if(int val = transTable.get(P.key())) {
        max = val + Position::MIN_SCORE - 1;
      }

      if(beta > max) {
        beta = max;                     // there is no need to keep beta above our max possible score.
        // 프루닝
        if(alpha >= beta) return beta;  // prune the exploration if the [alpha;beta] window is empty.
      }

      // 단순 탐색이 아닌 포지션 스코어 기반으로 트리 탐색 순서 조정.
      MoveSorter moves;

      // 데이터 삽입
      for(int i = Position::WIDTH; i--;) {
        if(uint64_t move = next & Position::column_mask(columnOrder[i])) {
          moves.add(move, P.moveScore(move));
        }
      }

      // 데이터 추출
      while(uint64_t next = moves.getNext()) {
        // 상대방의 착수. 미니맥스 원리로 상대방 스코어의 역수를 사용한다.
        Position P2(P);
        P2.play(next);
        int score = -negamax(P2, -beta, -alpha);

        // 프루닝
        if(score >= beta) return score;
        if(score > alpha) alpha = score;
      }

      // 해싱을 통해 퍼포먼스 향상
      transTable.put(P.key(), alpha - Position::MIN_SCORE + 1);
      return alpha;
    }

    public:

    void reset()
    {
      transTable.reset();
    }


    int solve(const Position &P, bool weak = false)
    {
      // 리커젼 탈출 조건으로 승리 여부 체크
      if(P.canWinNext())
        return (Position::WIDTH*Position::HEIGHT+1 - P.nbMoves())/2;
      int min = -(Position::WIDTH*Position::HEIGHT - P.nbMoves())/2;
      int max = (Position::WIDTH*Position::HEIGHT+1 - P.nbMoves())/2;
      if(weak) {
        min = -1;
        max = 1;
      }

      while(min < max) {
        int med = min + (max - min)/2;
        if(med <= 0 && min/2 < med) med = min/2;
        else if(med >= 0 && max/2 > med) med = max/2;
        int r = negamax(P, med, med + 1);
        if(r <= med) max = r;
        else min = r;
      }
      return min;
    }

    // 해싱을 위한 테이블 사이즈를 64MB로 고정. 사이즈는 반드시 소수여야 한다.
    Solver() : transTable(8388593) {
      reset();
    }

  };

}}


// Destory AI - 오리지널 코드
// 기본 게임 구현 및 스타일링, 예외처리 by 채희재
// 게임 상수 및 변수 초기화
Solver solver;
Position P;
int BOARD_COUNT[8];
bool ISCIRCLE = true;
bool GAME_END = false;
bool CAN_PLAY[8];
int RULE_ORDER[7] = {3, 4, 2, 5, 1, 6, 0};
// 휴리스틱 점수 출력을 위한 배열
int heuristic[7];
// 보드
char b[10][10];

// 선공을 결정하는 메소드
bool ISFIRST = false;
void askFirst() {
    std::cout << "먼저 두시겠습니까? (Y/N)   \e[38;5;99m입력 : \e[38;5;255m";
    std::string input;
    std::getline(std::cin, input);

    if (input.length() != 1) {
        std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m ";
        askFirst();
        return;
    } else {
        if (input[0] == 'Y' || input[0] == 'y') {
            ISFIRST = true;
        } else if (input[0] == 'N' || input[0] == 'n') {
            ISFIRST = false;
        } else {
            std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m ";
            askFirst();
            return;
        }
    }
    std::cout << "\n        \e[32m선공: O\e[38;5;255m   \e[33m후공: X\e[38;5;255m\n";
}

// 착수방법을 묻는 메소드
int METHOD = 0;
void askMethod() {
    std::cout << "착수할 방법을 선택해주세요. 1. Search Algorithm 2. Rule   \e[38;5;99m입력 : \e[38;5;255m";

    while(!(std::cin >> METHOD)){
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m 착수할 방법을 선택해주세요. 1. Search Algorithm 2. Rule   \e[38;5;99m입력 : \e[38;5;255m";
    }

    if (METHOD != 1 && METHOD !=2) {
        std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m ";
        askMethod();
        return;
    }
}

// 보드를 그리는 메소드
void draw() {
    std::cout << '\n';
    std::cout << "+ - - − - − - − - − - − - − - - +" << '\n';
    std::cout << "∣ 6 ∣ " << b[1][6] << " ∣ " << b[2][6] << " ∣ " << b[3][6] << " ∣ " << b[4][6] << " ∣ " << b[5][6] << " ∣ " << b[6][6] << " ∣ ";
    std::cout << b[7][6] << " ∣" << '\n';
    std::cout << "∣ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "∣ 5 ∣ " << b[1][5] << " ∣ " << b[2][5] << " ∣ " << b[3][5] << " ∣ " << b[4][5] << " ∣ " << b[5][5] << " ∣ " << b[6][5] << " ∣ ";
    std::cout << b[7][5] << " ∣" << '\n';
    std::cout << "∣ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "∣ 4 ∣ " << b[1][4] << " ∣ " << b[2][4] << " ∣ " << b[3][4] << " ∣ " << b[4][4] << " ∣ " << b[5][4] << " ∣ " << b[6][4] << " ∣ ";
    std::cout << b[7][4] << " ∣" << '\n';
    std::cout << "∣ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "∣ 3 ∣ " << b[1][3] << " ∣ " << b[2][3] << " ∣ " << b[3][3] << " ∣ " << b[4][3] << " ∣ " << b[5][3] << " ∣ " << b[6][3] << " ∣ ";
    std::cout << b[7][3] << " ∣" << '\n';
    std::cout << "∣ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "∣ 2 ∣ " << b[1][2] << " ∣ " << b[2][2] << " ∣ " << b[3][2] << " ∣ " << b[4][2] << " ∣ " << b[5][2] << " ∣ " << b[6][2] << " ∣ ";
    std::cout << b[7][2] << " ∣" << '\n';
    std::cout << "∣ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "∣ 1 ∣ " << b[1][1] << " ∣ " << b[2][1] << " ∣ " << b[3][1] << " ∣ " << b[4][1] << " ∣ " << b[5][1] << " ∣ " << b[6][1] << " ∣ ";
    std::cout << b[7][1] << " ∣" << '\n';
    std::cout << "+ - ∣ − + − + − + − + − + − + - ∣" << '\n';
    std::cout << "    ∣ 1 ∣ 2 ∣ 3 ∣ 4 ∣ 5 ∣ 6 ∣ 7 ∣" << '\n';
    std::cout << "    + − - − - − - − - − - − - - +" << '\n';
    std::cout << '\n';
}

// 보드를 빈칸으로 초기화
void initBoard() {
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++) {
            b[i][j] = ' ';
        }
    }
}

// 컬럼을 입력받는 메소드
int COL = 0;
void askCol() {
    std::cout << "착수할 컬럼을 선택해주세요. (1~7)   \e[38;5;99m입력 : \e[38;5;255m";
    while(!(std::cin >> COL)){
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m 착수할 컬럼을 선택해주세요. (1~7)   \e[38;5;99m입력 : \e[38;5;255m\n";
    }

    if (COL<1 || COL>7) {
        std::cout << "\e[38;5;196m잘못된 입력입니다.\e[38;5;255m ";
        askCol();
    }
}

// 수동 착수 - 상대방의 착수를 의미
void byHand() {
  askCol();
  if (P.canPlay(COL-1)) {

    // 이번 수로 승리하는지 체크
    if (P.isWinningMove(COL-1)) {
      GAME_END = true;
    }

    P.playCol(COL-1);

    if (ISCIRCLE) {
      b[COL][++BOARD_COUNT[COL]] = 'O';
    } else {
      b[COL][++BOARD_COUNT[COL]] = 'X';
    }

    ISCIRCLE = !ISCIRCLE;
    std::cout << "\e[36m >> 착수점\e[38;5;255m : " << "\e[93m(" << BOARD_COUNT[COL] << ',' << COL << ")\e[38;5;255m\n";

  } else {
    std::cout << "다른 컬럼을 선택해주세요. \n";
    byHand();
  }
}


// 자동 착수 - 게임 플레이어의 착수를 의미 by 채희재
// 1. Search Algorithm - 2분 시간제한 충족을 위해 5번 수까지는 룰 사용
// 수학적으로 선공이 중앙(4번 컬럼)에 착수할 시, 이후 양 플레이어가 이상적으로 착수하면
// 선공이 반드시 승리하는 것이 증명되어 있음.
// 또한, 행, 열의 중앙 지점에 가까운 곳을 차지하는 경우 Alignment를 만들기에 유리
// 따라서 3번 혹은 5번 컬럼을 첫 수로 두는 것이 승리에 유리한 것이 자명하나
// 구현의 편의성을 위해 3번으로 고정하였다.
// 현 상태에서 중앙부터 차례대로 탐색을 수행하여 스코어를 계산한 뒤, 시각적으로 보여준다.
//

int temp, max;
int cOrder[7] = {3, 4, 2, 5, 1, 6, 0};
void bySearch() {
  std::cout << "\e[92m";
  for (int i=0; i<7; i++) {
    // 휴리스틱 계산이 불가능한 경우를 -100으로 산정.
    heuristic[i] = 100;
  }

  for (int col=0; col<7; col++) {

    if (P.canPlay(cOrder[col])) {

      if (P.isWinningMove(cOrder[col])) {
        std::cout << cOrder[col]+1 << "번 컬럼에 착수하면 바로 승리할 수 있습니다.\n";
        COL = cOrder[col]+1;
        return;
      }

      // 임시 포지션
      Position T(P);
      T.playCol(cOrder[col]);

      temp = solver.solve(T, false);
      heuristic[cOrder[col]] = temp;
    }
  }

  max = -100;
  std::cout << "\e[38;5;255m" << '\n';
  draw();
  std::cout << "\e[92m";
  std::cout << "    |";
  for (int i=0; i<7; i++) {
    if (max < -heuristic[i]) {
      max = -heuristic[i];
      COL = i+1;
    }

    // 휴리스틱을 정상적으로 계산했다면,
    if (heuristic[i] != 100) {
      // 10 이상이면 " 10|"
      if (-heuristic[i] >= 10) {
        std::cout << ' ' << -heuristic[i] << '|';
      // 0 이면 "  0|"
      } else if (heuristic[i] == 0) {
        std::cout << "  " << -heuristic[i] << '|';
      // -10 이하면 "-10|"
      } else if (-heuristic[i] <= -10) {
        std::cout << -heuristic[i] << '|';
      } else if (-heuristic[i] < 0) {
        std::cout << ' ' << -heuristic[i] << '|';
      } else {
        std::cout << "  " << -heuristic[i] << '|';
      }
    } else {
      std::cout << "   |";
    }
  }
  std::cout << "\e[38;5;255m" << '\n';

}

// 2. Rule by 문선미, 이태훈, 채희재
// 초반 5번 수까지는 수가 크게 많지 않으므로 A -> B 이다 형식으로 강제로 착수한다.
// 6번 수부터는 이러한 케이스가 기하급수적으로 증가하므로 다른 룰을 적용한다.
// 두었을 때 4개의 돌이 연결되어 승리하는 수를 가장 먼저 고려한다.
// 두지 않아 패배하게 되는 경우는 막는다.
// 그 외 대부분은 상대방의 착수 위에 바로 두는 것을 기본으로 한다.
// 만약 컬럼이 모두 꽉 찼다면 중앙이 유리하다는 전제하에 중앙에 가까운 컬럼 위주로 착수한다.
// 아주 예외적인 케이스는 개별적으로 등록하여 A -> B 이다 형식으로 강제로 착수한다.
//
void byRule() {
  std::cout << "\e[92m";
  // 첫 수가 Rule.
  if (P.nbMoves() == 0) {
    std::cout << "\nRule - 중앙일수록 좋으나 제한조건이 있다. 첫 수는 3에 둔다.\n";
    COL = 3;

    // 수-2 가 룰.
  } else if (P.nbMoves() == 1) {
    if (b[1][1]!=' ' || b[2][1]!=' ' || b[3][1]!=' ') {
      // 상대가 왼편에 둔 경우,
      if (b[2][1]!=' ') {
        std::cout << "\nRule - 상대 첫 수가 2 || 6 인 경우, 가운데 방향으로 붙여서 둔다.(3 || 5)\n";
        COL = 3;
      } else {
        std::cout << "\nRule - 상대 첫 수가 2 || 6 이 아닌 경우, 중앙에 둔다.\n";
        COL = 4;
      }
    } else {
      // 상대가 우측에 둔 경우,
      if (b[6][1]!=' ') {
        std::cout << "\nRule - 상대 첫 수가 2 || 6 인 경우, 가운데 방향으로 붙여서 둔다.(3 || 5)\n";
        COL = 5;
      } else {
        std::cout << "\nRule - 상대 첫 수가 2 || 6 이 아닌 경우, 중앙에 둔다.\n";
        COL = 4;
      }
    }
    // 수-3 가 룰
  } else if (P.nbMoves() == 2) {

    if (b[1][1]!=' ' || b[3][2]!=' ' || b[4][1]!=' ' || b[6][1]!=' ' || b[7][1]!=' ') {
      std::cout << "\nRule - 상대 2번 수가 1 || 3 || 4 || 6 || 7 인 경우, 중앙에 둔다.\n";
      COL = 4;
    } else if (b[2][1]!=' ') {
      std::cout << "\nRule - 상대 2번 수가 2 인 경우, 6에 둔다.\n";
      COL = 6;
    } else if (b[5][1]!=' ') {
      std::cout << "\nRule - 상대 2번 수가 5 인 경우, 3에 둔다.\n";
      COL = 3;
    }

  } else if (P.nbMoves() == 3) {

     if (b[1][1] == 'O' || b[7][1] == 'O') {
        std::cout << "\nRule - 4번 수가 룰인 경우, 1 || 7 에 O가 있다면 중앙에 둔다.\n";
        COL = 4;
     }
     else if (b[2][1] == 'O') {
        if (b[2][2] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 2 에 O가 연속 2개 있다면 2에 둔다.\n";
           COL = 2;
        }
        else if (b[3][2] == 'O' || b[5][1] == 'O' || b[6][1] == 'O' || b[7][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 2에 O가 있으며 [ (2,3) || 5 || 6 || 7 ] 에 O가 있다면 3에 둔다.\n";
           COL = 3;
        }
        else if (b[4][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 2에 O가 있으며 4에도 O가 있다면 4에 둔다.\n";
           COL = 4;
        }
     }
     else if (b[3][1] == 'O') {
        if (b[3][2] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 3 에 O가 연속 2개 있다면 3에 둔다.\n";
           COL = 3;
        }
        else if (b[4][2] == 'O' || b[5][1] == 'O' || b[6][1] == 'O' || b[7][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 3에 O가 있으며 [ (2,4) || 5 || 6 || 7 ] 에 O가 있다면 4에 둔다.\n";
           COL = 4;
        }
     }
     else if (b[5][1] == 'O') {
        if (b[5][2] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 5 에 O가 연속 2개 있다면 5에 둔다.\n";
           COL = 5;
        }
        else if (b[4][2] == 'O' || b[6][1] == 'O' || b[7][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 5에 O가 있으며 [ (2,4) || 6 || 7 ] 에 O가 있다면 4에 둔다.\n";
           COL = 4;
        }
     }
     else if (b[6][1] == 'O') {
        if (b[6][2] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 6 에 O가 연속 2개 있다면 6에 둔다.\n";
           COL = 6;
        }
        else if (b[5][2] == 'O' || b[7][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 6에 O가 있으며 [ (2,5) || 7 ] 에 O가 있다면 5에 둔다.\n";
           COL = 5;
        }
        else if (b[4][1] == 'O') {
           std::cout << "\nRule - 4번 수가 룰인 경우, 6에 O가 있으며 4에도 O가 있다면 4에 둔다.\n";
           COL = 4;
        }
    } else {
       std::cout << "\nRule - 혹시 모를 상황에는 4에 둔다.\n";
       COL = 4;
    }
  }
  else if (P.nbMoves() == 4) {
    if(b[1][1]=='X' && b[4][1]=='O'){
      if(b[1][2] == 'X' || b[3][2]=='X' || b[4][2]=='X' || b[5][1]=='X' ||b[7][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 1에 X가 있으며 (2,1) || (2,3) || (2,4) || (1,5) || (1,7) 에 X가 있다면 5에 둔다.\n";
        COL = 5;
      }
      else if(b[2][1] == 'X' || b[6][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 1에 X가 있으며 (1,2) || (1,6) 에 X가 있다면 2에 둔다.\n";
        COL = 2;
      }
    }
    else if(b[2][1]=='X'&& b[6][1]=='O'){
      if(b[1][1]=='X' || b[3][2]=='X'|| b[5][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 2에 X가 있으며 (1,1) || (2,3) || (1,5) 에 X가 있다면 3에 둔다.\n";
        COL = 3;
      }
      else if(b[2][2]=='X' || b[4][1]=='X' || b[6][2]=='X' || b[7][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 2에 X가 있으며 (2,2) || (1,4) || (2,6) || (1,7) 에 X가 있다면 6에 둔다.\n";
        COL = 6;
      }
    }
    else if(b[3][2]=='X' && b[4][1]=='O'){
      if(b[1][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, (2,3)에 X가 있으며 1에 X가 있다면 5에 둔다.\n";
        COL = 5;
      }
      else if(b[2][1]=='X' || b[3][3]=='X' || b[4][2]=='X' || b[5][1]=='X' ||b[6][1]=='X'||b[7][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, (2,3)에 X가 있으며 1에 X가 없다면 2에 둔다.\n";
        COL = 2;
      }
    }

    else if(b[4][1]=='X' && b[4][2]=='O'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, (1,4)에 X가 있으며 (2,4)에 O가 있다면 항상 4에 둔다.\n";
        COL = 4;
    }

    else if(b[5][1]=='X'&&b[3][2]=='O'){
      if(b[1][1]=='X' || b[2][1]=='X' ||b[4][1]=='X' ||b[5][2]=='X'||b[6][1]=='X'|| b[7][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, (1,5)에 X가 있으며 (3,3)에 X가 없다면 3에 둔다.\n";
        COL = 3;
      }
      else if(b[3][3]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, (1,5)에 X가 있으며 (3,3)에 X가 있다면 5에 둔다.\n";
        COL = 5;
      }
    }

    else if(b[6][1]=='X' && b[4][1]=='O'){
      if(b[1][1]=='X' || b[3][2]=='X'|| b[4][2]=='X'||b[6][2]=='X'||b[7][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 6에 X가 있으며 (1,2)과 (1,5)에 X가 없다면 2에 둔다.\n";
        COL = 2;
      }
      else if(b[2][1]=='X' ){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 6에 X가 있으며 (1,2)에 X가 있다면 6에 둔다.\n";
        COL = 6;
      }
      else if(b[5][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 6에 X가 있으며 (1,5)에 X가 있다면 3에 둔다.\n";
        COL = 3;
      }
    }
    else if(b[7][1]=='X' && b[4][1]=='O') {
      if(b[1][1]=='X'||b[3][2]=='X'||b[4][2]=='X'||b[5][1]=='X'||b[7][2]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 7에 X가 있으며 (1,2)과 (1,6)에 X가 없다면 5에 둔다.\n";
        COL = 5;
      }
      else if(b[6][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 7에 X가 있으며 (1,6)에 X가 있다면 2에 둔다.\n";
        COL = 2;
      }
      else if(b[2][1]=='X'){
        std::cout <<"\nRule - 5번 수가 룰인 경우, 7에 X가 있으며 (1,2)에 X가 있다면 3에 둔다.\n";
        COL = 3;
      }
    }
  } else {
    // 채희재
    // 룰 기반으로 COL 을 결정해야 한다. 실제 착수는 COL 기반으로 다른 곳에서 착수하게 된다.

    // 컬럼 초기화
    for (int i=0; i<8; i++) {
      CAN_PLAY[i] = false;
    }

    // 먼저 착수가 가능한 컬럼을 계산. 바로 이길 수 있다면 이긴다.
    for (int i=0; i<7; i++) {
      if (P.canPlay(i)) {
        CAN_PLAY[i+1] = true;
        if (P.isWinningMove(i)) {
          COL = i+1;
          std::cout <<"\nRule - 이번 착수로 바로 승리할 수 있다면 착수한다.\n";
          std::cout << "\e[38;5;255m";
          return;
        }
      }
    }

    // 착수 가능한 곳 마킹
    uint64_t possible_mask = P.possible();
    // 상대방이 착수 시 이기는 곳 마킹
    uint64_t opponent_win = P.opponent_winning_position();
    // 내가 둘 수 있는 데 두지 않을 경우, 상대가 두어서 승리하므로, 해당 위치는 강제하여 착수한다.
    uint64_t forced_moves = possible_mask & opponent_win;

    if (forced_moves) {
      for (int i=0; i<7; i++) {
        // 강제 착수가 어떤 컬럼에서 진행되어야 하는지 체크
        if (P.column_mask(i) & forced_moves) {
          COL = i+1;
          std::cout <<"\nRule - 이번 착수로 막지 않아 패배한다면, 패배하기 전에 막는다.\n";
          break;
        }
      }
      std::cout << "\e[38;5;255m";
      return;
    }

    // 대부분 마지막 착수 바로 위에 두는데, 해당 칼럼이 착수 불가능하다면,
    if (!P.canPlay(COL-1)) {
      for (int i=0; i<7; i++) {
        if (P.canPlay(RULE_ORDER[i])) {
          COL = RULE_ORDER[i]+1;
          std::cout <<"\nRule - 대부분 마지막 착수 바로 위에 두는데, 해당 칼럼이 착수 불가능하다면, 착수 가능한 중앙에 가까운 수에 둔다.\n";
          break;
        }
      }
      std::cout << "\e[38;5;255m";
      return;
    }

    // by 이태훈
    if (P.nbMoves() == 5) {
      if (b[2][1]=='O'&&b[2][2]=='O'&&b[2][3]=='X'&&b[2][4]=='O'&&b[3][1]=='X') {
        std::cout <<"\nRule - ㄴ 모양일 때, 3에 둔다.\n";
        COL = 3;
      }
      std::cout << "\e[38;5;255m";
      return;
    }

    if (P.nbMoves() ==6){
      if
      (b[3][1]=='O'&& b[4][2]=='O' && b[4][4]=='O' && b[4][1]=='X' && b[4][3]=='X'&& b[4][5]=='X'){
        std::cout <<"\nRule - ㄴ 모양일 때, 6에 둔다.\n";
        COL = 6;
      }
      std::cout << "\e[38;5;255m";
      return;
    }

    // by 채희재
    if (P.nbMoves() == 9) {
      if (b[2][1]=='O'&&b[2][2]=='O'&&b[2][3]=='X'&&b[2][4]=='O'&&b[3][1]=='X'&&b[3][2]=='X'&&b[3][3]=='O'&&b[3][5]=='O') {
        std::cout <<"\nRule - 따봉 모양일 때, 2에 둔다.\n";
        COL = 2;
      }
      std::cout << "\e[38;5;255m";
      return;
    }

    // by 이태훈
    if (P.nbMoves() == 6) {
      if (b[1][1]=='X'&&b[3][2]=='X'&&b[5][1]=='X'&&b[2][1]=='O'&&b[3][1]=='O'&&b[4][1]=='O') {
        // ㅗ 모양일 때, 가운데에 둔다. 1
        std::cout <<"\nRule - ㅗ 모양일 때, 가운데에 둔다.\n";
        COL = 4;
        std::cout << "\e[38;5;255m";
        return;

      } else if (b[3][1]=='X'&&b[5][2]=='X'&&b[7][1]=='X'&&b[4][1]=='O'&&b[5][1]=='O'&&b[6][1]=='O') {
        // ㅗ 모양일 때, 가운데에 둔다. 2
        std::cout <<"\nRule - ㅗ 모양일 때, 가운데에 둔다.\n";
        COL = 4;
        std::cout << "\e[38;5;255m";
        return;
      }
    } else if (P.nbMoves() == 8) {
      if (b[1][1]=='X'&&b[3][2]=='X'&&b[5][1]=='X'&&b[2][1]=='O'&&b[3][1]=='O'&&b[4][1]=='O'&&b[3][4]=='X') {
        // ㅗ 모양일 때, 가운데에 둔다. 1
        std::cout <<"\nRule - ㅗ 모양일 때, 가운데에 둔다.\n";
        COL = 4;
        std::cout << "\e[38;5;255m";
        return;

      } else if (b[3][1]=='X'&&b[5][2]=='X'&&b[7][1]=='X'&&b[4][1]=='O'&&b[5][1]=='O'&&b[6][1]=='O'&&b[5][4]=='X') {
        // ㅗ 모양일 때, 가운데에 둔다. 2
        std::cout <<"\nRule - ㅗ 모양일 때, 가운데에 둔다.\n";
        COL = 4;
        std::cout << "\e[38;5;255m";
        return;
      }
    }

    // by 문선미
    std::cout <<"\nRule - 상대방이 둔 곳 바로 위에 둔다.\n";

  }

  std::cout << "\e[38;5;255m";
}


// 자동착수로 계산된 COL 을 사용해 실제로 착수한다.
void playByAuto(int col) {
  // 이번 수로 승리하는지 체크
  if (P.isWinningMove(col-1)) {
    GAME_END = true;
  }

  P.playCol(col-1);

  if (ISCIRCLE) {
    b[col][++BOARD_COUNT[col]] = 'O';
  } else {
    b[col][++BOARD_COUNT[col]] = 'X';
  }

  std::cout << "\e[36m >> 착수점\e[38;5;255m : " << "\e[93m(" << BOARD_COUNT[col] << ',' << col << ")\e[38;5;255m\n";
  ISCIRCLE = !ISCIRCLE;
}

// 게임 종료 여부 판단.
bool checkEnd() {
  if (GAME_END) {
    if (ISCIRCLE) {
      draw();
      std::cout << "\e[33m\n    X 가 승리하였습니다! \e[38;5;255m\n\n";
      return true;
    } else {
      draw();
      std::cout << "\e[32m\n    O 가 승리하였습니다! \e[38;5;255m\n\n";
      return true;
    }
  } else {
    return false;
  }
}

// 메인 프로그램 구현 by 채희재
int main() {
  std::cout << "\e[38;5;255m";
  std::cout << "\n\e[38;5;198mDestroy AI - Connect4 Solver\e[38;5;255m\n";
  std::cout << "                             by \e[38;5;117m채희재 이태훈 문선미\e[38;5;255m\n\n";
  initBoard();
  askFirst();

  // 후수인 경우
  if (!ISFIRST) {
    draw();
    byHand();
  }

  while(1) {
    // 자동 착수
    draw();

    askMethod();
    if (METHOD == 1) {
      // 서치 기반. 단 5수까지는 룰을 사용해 빠르게 착수.
      if (P.nbMoves() < 5) {
        byRule();
      } else {
        bySearch();
      }
    } else {
      // 룰 기반
      byRule();
    }
    playByAuto(COL);

    // 승리 여부 체크
    if (checkEnd()) {
      break;
    }
    // 무승부 체크
    if (P.nbMoves() == 42) {
      draw();
      std::cout << "무승부입니다. \n";
      break;
    }

    // 수동 착수
    draw();
    byHand();

    // 승리 여부 체크
    if (checkEnd()) {
      break;
    }
    // 무승부 체크
    if (P.nbMoves() == 42) {
      draw();
      std::cout << "무승부입니다. \n";
      break;
    }
  }
}
