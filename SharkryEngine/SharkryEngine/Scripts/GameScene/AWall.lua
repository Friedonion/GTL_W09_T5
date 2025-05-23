
-- Template은 AActor라는 가정 하에 작동.

local ReturnTable = {} -- Return용 table. cpp에서 Table 단위로 객체 관리.

local FVector = EngineTypes.FVector -- EngineTypes로 등록된 FVector local로 선언.

ReturnTable.LifeTimer = 0.0 -- 수명 타이머 추가

-- BeginPlay: Actor가 처음 활성화될 때 호출
function ReturnTable:BeginPlay()

    --print("BeginPlay ", self.Name) -- Table에 등록해 준 Name 출력.
    self.this.Speed = 20
    self.this.VarientValue = math.random(-3, 3)
    self.LifeTimer = 0.0


end

-- Tick: 매 프레임마다 호출
function ReturnTable:Tick(DeltaTime)
    
    
    -- 기본적으로 Table로 등록된 변수는 self, Class usertype으로 선언된 변수는 self.this로 불러오도록 설정됨.
    -- sol::property로 등록된 변수는 변수 사용으로 getter, setter 등록이 되어 .(dot) 으로 접근가능하고
    -- 바로 등록된 경우에는 PropertyName() 과 같이 함수 형태로 호출되어야 함.
    local NewLoc = self.this.ActorLocation -- 현재 Actor Location 변수로 저장
    NewLoc.X = NewLoc.X - self.this.Speed * DeltaTime -- X 방향으로 이동하도록 선언.
    self.this.ActorLocation = NewLoc

    self.LifeTimer = self.LifeTimer + DeltaTime
    if self.LifeTimer >= 6.0 then
        self.this:Destroy()
    end
end

-- EndPlay: Actor가 파괴되거나 레벨이 전환될 때 호출
function ReturnTable:EndPlay(EndPlayReason)
    -- print("[Lua] EndPlay called. Reason:", EndPlayReason) -- EndPlayReason Type 등록된 이후 사용 가능.
    --print("EndPlay")
end

function ReturnTable:OnOverlapBullet(Other)
    self.this.VarientValue = self.this.VarientValue + 1;
end

return ReturnTable




