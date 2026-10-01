# 9th-Team2-Final-Project

# 개발 환경
Engine: Unreal Engine 5.8

IDE: JetBrains Rider

Version Control: SourceTree / GitHub Desktop

# 브랜치 운영
본 프로젝트는 dev 브랜치를 기준으로 개발을 진행합니다.

main : 안정화 및 최종본 관리

dev : 실제 개발 및 작업물 통합

작업 시작 전 반드시 dev 브랜치를 최신 상태로 업데이트한 뒤 작업해주세요.

# ⚠️ 협업 시 필수 확인 사항
작업 전 반드시 팀원에게 공유해주세요.

Unreal Engine 프로젝트 특성상 동일한 Level, Blueprint, DataAsset 등의 파일을 여러 명이 동시에 수정할 경우 충돌 해결이 어렵습니다.

따라서 작업이 겹치지 않도록 사전 소통을 필수로 합니다.

작업 시작 전 자신이 수정할 기능 및 파일을 팀원에게 공유합니다.

다른 팀원이 동일한 파일을 작업하고 있는지 확인합니다.

작업이 겹치는 경우 담당자끼리 먼저 조율한 후 작업합니다.

dev 최신화(Pull) 후 작업을 시작합니다.

Push 전 변경된 파일을 다시 확인합니다.

다른 팀원의 작업물을 임의로 수정하거나 덮어쓰지 않습니다.

작업 시작 → 팀원 공유 → 중복 작업 확인 → dev 최신화 → 작업 → Commit → Push

특히 Level(.umap), Blueprint 및 Unreal Asset(.uasset) 작업은 동시에 수정하지 않도록 주의해주세요.

# Commit
커밋 메시지는 어떤 작업을 했는지 알아볼 수 있도록 작성합니다.

예시:

Feat: GameMode 기본 구조 구현

Fix: 상호작용 조건 오류 수정

Update: 호텔 로비 레벨 수정

Add: 조사 아이템 DataAsset 추가
