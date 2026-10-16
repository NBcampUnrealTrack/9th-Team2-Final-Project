# 1막 NPC 대화 골격 연동 메모

실제 시나리오 대사 대신 임시 문구만 사용한다. SigolYouth와 WorkSenior 파일 및 전화기 보고 코드는 변경하지 않는다.

역할은 Caretaker=관리인, Manager=지배인이다. BP 이름과 NPCId는 변경하지 않는다.

## 확인된 NPC ID

| 대상 | 확인 결과 | CSV의 DialogueID |
| --- | --- | --- |
| Bellboy | BP_Bellboy의 NPCId 직렬화 값이 Bellboy | Bellboy |
| Caretaker | ALCOCaretaker 생성자의 NPCId가 Caretaker | Caretaker |
| SigolYouth | ALCOSigolYouth 생성자의 NPCId가 SigolYouth | SigolYouth (기존 파일 유지) |
| Cleaner | LCONPCBase 상속, BP에 NPCId override 없음. 기본 None | Cleaner (연결 대기) |
| FrontDesk | LCONPCBase 상속, BP에 NPCId override 없음. 기본 None | FrontDesk (연결 대기) |
| Manager | LCONPCBase 상속, BP에 NPCId override 없음. 기본 None | Manager (연결 대기) |
| OldProfessor | LCONPCBase 상속, BP에 NPCId override 없음. 기본 None | OldProfessor (연결 대기) |

마지막 네 ID는 기존 실제 ID라고 추정해 치환한 값이 아니라, 요청된 NPC용 CSV 연결 대상 이름이다. 해당 액터의 NPCId가 설정되어야 기존 GetNPCId() = DialogueID 경로로 열 수 있다. UI에서 액터 이름으로 ID를 추정하거나 NPCId를 덮어쓰지 않는다. 조사한 맵에는 NPCId override가 발견되지 않았다.

위 표는 최초 디스크 에셋 조사 결과이며 이후 사용자가 에디터에서 설정한 배치 액터 값을 대신하지 않는다. 사용자는 Caretaker의 NPCId=Caretaker, Can Talk=true 및 다른 NPC의 첫/반복 대화 정상 실행을 확인했다. NPCId나 Can Talk를 다시 추정하여 UI에서 덮어쓰지 않는다.

## 기존 대화/획득 정책 재사용

- FIRST_01 정상 완료 시 LCODialogueSubsystem의 기존 첫 대화 트랜잭션을 확정한다. 중단하면 완료 집합에 추가하지 않는다. DEFAULT_01 이후에는 선택지로 이동한다.
- SigolYouth의 Condition은 비어 있으면 항상 표시, 단일 ID이면 보유 여부 검사이다. AND/OR/NOT 등 조건 문법은 추가하지 않는다.
- 여섯 새 DialogueID만 Condition을 영속 수첩 ULCONotebookSubsystem::HasEntry로 조회한다. E 항목은 기존 인벤토리 HasItem/NotebookComponent HasEvidence 경로도 허용한다. T/K 항목은 실제 수첩 기록을 요구한다. SigolYouth/WorkSenior의 기존 조회 정책은 유지한다.
- 획득한 증언의 선택지도 조건을 만족하는 동안 계속 표시한다. 이미 아는 증언을 숨기는 별도 정책은 추가하지 않는다.
- UnlockFlag는 기존 선택 열이다. 대사 표시 시 Story::TryUnlockByName -> 조건 검사 -> 플래그 등록 -> DT_NotebookData의 행을 Notebook::AddEntry로 기록한다. 같은 플래그는 중복 지급하지 않는다.
- 증언 지급은 대사 표시 시점이다. 이후 대화를 중단해도 이미 지급된 증언을 회수하지 않는다. 첫 대화 완료 여부와 별개인 기존 정책이다.
- OldProfessor의 T01과 Manager의 T03은 FIRST_01에서 지급하지 않는다. 반복 대화에서 조건 없는 기본 증언 선택지를 고른 후 T01_01/T03_01 표시 시에만 지급한다. 이전 구현으로 이미 지급된 기록은 삭제하지 않는다.
- 기존 DT_NotebookData에서 T01~T11, K01~K08 식별자를 확인했다. 테이블/행 누락 시 Story 플래그만 생기고 수첩 기록은 실패할 수 있다. UI에서 임시 수첩 항목을 만들어 우회하지 않는다.
- 증언/추리 플래그, 수첩 항목, 첫 대화 완료 목록은 기존 SaveManager 저장/복원 및 새 게임 초기화 경로를 사용한다. 추리 조합은 기존 수첩/Story::TryDeduce에 맡긴다.

## 선택지와 지급 관계

| DialogueID | 선택지 조건 -> 대사 표시 시 지급 |
| --- | --- |
| OldProfessor | 기본 -> T01, K01 -> T02, T01 -> T08 |
| Caretaker | E02 -> T05 |
| Bellboy | E01 -> T04 |
| Manager | 기본 -> T03, K02 -> T09 |
| FrontDesk | E01 -> T06, E03 -> T10, T09 -> T11 |
| Cleaner | E06 -> T07 |

조건부 대사는 DEFAULT_CHOICES로 돌아온다. 조건을 통과한 실제 질문이 있으면 기존 대화 종료 선택지도 표시한다. 질문이 하나도 없으면 재대화의 선택지 단계 대신 NPC_NO_QUESTIONS 한 줄을 표시하고 다음 진행 입력으로 정상 종료한다. FIRST_01에서 바로 정상 종료하므로 첫 대화에 선택지 탐색을 강제하지 않는다.

이 공통 처리는 ActiveNPC가 있는 반복 대화에만 적용한다. BYE 및 명시적인 종료 선택지는 질문 수에서 제외하고 나머지 선택지는 기존 Condition을 통과한 경우만 센다. 특정 DEFAULT_01 이름에 의존하지 않으며, 선택지가 없는 연속 대사와 WorkSenior 등 NPC 없는 전화 대화는 유지한다. 중간 대사에 UnlockFlag/ReportKeyword가 있으면 해당 대사를 건너뛰지 않는다. 공통 종료 문장은 임시 런타임 Line 노드로 기존 타이핑/정상 완료 경로를 이용하며, 새 저장 기록은 만들지 않는다.

## 지배인 보고 및 엔딩 연결

새 CSV 선택 열 ReportKeyword는 Line 노드의 정상 완료에만 적용한다. 지배인 Manager 이외에는 실행하지 않는다. 관리인 Caretaker에는 보고 분기가 없다. UnlockFlag와 별개이며 신규 플래그를 지급하지 않는다. 기존 CSV에는 열이 없어 기본 None으로 처리된다.

- K07 보유: 조사 종결 보고 선택지를 표시한다. K03 보유 여부를 검사하지 않는다.
- K05 보유: 별관 407호 조사 요청 선택지를 표시한다.
- K05/K07 동시 보유: 두 선택지를 동시에 표시한다. 자동 우선순위는 없다.
- Manager의 REPORT_K07 선택지는 한 개만 유지한다. 선택 시 AlternateCondition=K03을 기존 Condition 평가로 검사하여 보유하면 AlternateTargetNodeID=REPORT_K03_01로 간다. 없으면 기존 REPORT_K07_01로 간다. 이 두 열은 선택 열이며 기존 CSV는 비어 있는 것으로 처리한다.
- REPORT_K03_01~05는 머뭇거린 뒤 아무것도 아니라고 말하고 정상 종료하는 대사다. UnlockFlag/ReportKeyword가 없으므로 증언 지급, K07 보고, 엔딩 호출을 하지 않는다. K03은 기존 영속 수첩의 보유 상태로 판정한다.
- ReportKeyword=K05: 대화 정상 종료/완료 기록 방송 이후 기존 CanReportToManager(K05)를 재검사하고 ReportToManager(K05)를 호출한다. 기존 Story가 Chapter1을 완료하고 Chapter2를 시작하며 GameMode가 OnReportResolved를 받아 맵 이동/자동 저장한다. 중단 시 이 API는 호출하지 않는다.
- 기존 GameMode는 Chapter2StartLevel이 설정되면 맵 이동 후 자동 저장한다. 미설정이면 이동 없이 현 위치에서 저장한다. 별도 선택 화면은 없다. 새 챕터 진입 전에 저장 완료를 기다리는 구조는 아니다. 목적 맵/407호 입장/허가 장치 설정은 이번 작업 범위 밖이다.
- ReportKeyword=K07: K03 미보유 시의 기존 REPORT_K07_01이 정상 종료되고 미연결 경고만 출력한다. 향후 END 3 연결용 처리 구조는 유지하며 새 엔딩을 만들지 않는다. 기존 Story의 ReportToManager(K07)와 엔딩 체크포인트 수명 주기는 별도 연동 대상이다. K03 보유 시에는 해당 노드 자체에 진입하지 않는다.
- 기존 END 3 처리는 GameMode 로그뿐이며, END 3 화면, 보고 직전 자동 저장 완료 대기, 엔딩 종료 후 체크포인트 복원 API는 확인되지 않았다. Test_Outdoor 레벨 BP의 ReportToManager/SaveGame 참조도 확인했으나 전용 엔딩 수명 주기를 확인할 근거는 없었다. 따라서 K07 보고와 해당 수명 주기는 미연결이다. 기존 SaveManager의 일반 저장/이어하기만으로 전용 엔딩 체크포인트가 완성된 것으로 간주하지 않는다.

보험 조사 루트는 K07 선택지까지, 과거 사건 루트는 K05 선택지와 기존 1막 완료 API까지 연결한다. K03은 기존 추리 항목으로 남으며 두 보고 선택지의 표시를 차단하지 않는다.

## 허용 범위 밖 후속 작업

1. Caretaker 상호작용 컴포넌트/충돌 설정 확인. 다른 NPC의 ID/첫 대화/반복 대화는 사용자 확인상 정상이다.
2. K03 미보유 REPORT_K07_01의 END 3 연동. K03 보유 분기는 UI 대화로 종료되며 Story 보고 API를 호출하지 않는다. 기존 스토리 규칙은 수정하지 않았다.
3. END 3 화면과 보고 전 체크포인트 저장/복원 계약 제공. 해당 계약이 정해진 뒤 UI에 연결 가능하다.
4. 2막 목적 맵과 407호 입장/허가 장치 확인. 저장 후 이동 순서가 필수라면 기존 GameMode/전환 API의 계약 변경이 필요하다.

빌드 및 Unreal Editor는 실행하지 않는다. CSV 그래프/참조/조건표와 기존 코드 경로만 정적으로 검증한다.

## Caretaker 상호작용 및 선택지 미표시 후속 조사

- 디스크의 Content/WB/NPC/BP_Caretaker는 ALCOCaretaker -> ALCONPCBase를 상속한다. 두 C++ 클래스 모두 ULCOInteractComponent를 생성하지 않으며 해당 BP에도 LCOInteract 컴포넌트가 없다. 정상 NPC인 BP_Bellboy/OldProfessor에는 LCOInteractComponent가 있다. Content/WB/BP_Caretaker는 해당 NPC 폴더 에셋을 가리키는 리다이렉터다.
- 플레이어는 Visibility 채널의 첫 충돌 컴포넌트 자체를 ULCOInteractComponent로 Cast한다. 캡슐/스켈레탈 메시를 맞히면 Owner가 NPC여도 상호작용할 수 없다. UI 서브시스템은 포커스된 컴포넌트의 OnInteracted를 통해 시작하므로 이 단계 전의 실패를 UI에서 우회하지 않는다.
- 에디터에서 실제 사용 중인 BP_Caretaker의 Components에 LCOInteractComponent를 추가하고, 충돌 가능한 Static Mesh와 접근 가능한 위치/크기를 지정한다. Interaction Enabled=true, Linked Target=None(Owner 사용), Collision Enabled=Query Only, Visibility=Block으로 설정한다. 캡슐/캐릭터 메시의 Visibility는 Ignore로 두어 앞에서 트레이스를 가로막지 않게 한다. NPCId=Caretaker, Can Talk=true 및 배치 인스턴스 override도 확인한다. 다른 충돌 채널과 AI 설정은 유지한다.
- DEFAULT_01 -> DEFAULT_CHOICES, Choice 행 병합, 무조건 BYE 선택지, 조건 없는 T01/T03 선택지, 선택지 클래스 경로 및 위젯 바인딩 이름은 정적으로 정상이다. WBP_Dialogue의 ChoicesPanel은 VerticalBox이며 Border_Choices -> ScrollBox -> ChoicesPanel 구조다. Border_Choices의 초기 Collapsed는 C++에서 Visible로 바뀐다. WBP_DialogueChoice의 높이는 45이고 Button_Choice/Text_Choice가 존재한다. CSV 조건 부족만으로 모든 선택지가 사라지는 구조가 아니다.
- 실제 미표시 원인은 실행 로그/화면의 런타임 상태 없이 확정하지 않았다. 새 여섯 NPC에 한해 선택지 표시 후 레이아웃을 재계산하고 생성 수/노드/위젯 클래스 로그를 남긴다. ChoicesPanel 바인딩 누락과 선택지 생성 실패도 경고한다. SigolYouth/WorkSenior의 레이아웃 경로는 변경하지 않는다.
- 에디터 확인: WBP_Dialogue의 ChoiceWidgetClass는 WBP_DialogueChoice, ChoicesPanel/Border_Choices 이름과 Is Variable 확인. 부모 위젯의 Collapsed/Hidden, Render Opacity 0, 0 크기 또는 클리핑 여부 확인. WBP_DialogueChoice는 LCODialogueChoiceWidget 상속과 Button_Choice/Text_Choice 이름을 유지한다. 성공 로그의 Visible/Children이 양수인데 보이지 않으면 CSV가 아니라 해당 런타임 배치/스타일/애니메이션을 확인한다.
