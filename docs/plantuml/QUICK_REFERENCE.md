# PlantUML Quick Reference Cheat Sheet

## Basic Diagram Types

### 1. Component Diagram
```plantuml
@startuml
!theme cerulean

[Component A] --> [Component B] : uses
[Component B] --> [Component C] : calls

package "Module" {
  [SubComponent 1]
  [SubComponent 2]
}

@enduml
```

### 2. Sequence Diagram
```plantuml
@startuml
!theme cerulean

actor User
participant System
database Database

User -> System : Request
activate System
System -> Database : Query
activate Database
Database --> System : Result
deactivate Database
System --> User : Response
deactivate System

@enduml
```

### 3. State Diagram
```plantuml
@startuml
!theme cerulean

[*] --> Idle
Idle --> Running : Start
Running --> Paused : Pause
Paused --> Running : Resume
Running --> [*] : Stop

note right of Running
  Active processing
end note

@enduml
```

### 4. Activity Diagram (Data Flow)
```plantuml
@startuml
!theme cerulean

start
:Input Data;
:Process Data;
if (Valid?) then (yes)
  :Output Result;
else (no)
  :Log Error;
endif
stop

@enduml
```

### 5. Class Diagram (Structures)
```plantuml
@startuml
!theme cerulean

class MotorController {
  - position : int32_t
  - velocity : float
  + Start() : void
  + Stop() : void
  + SetVelocity(v : float) : void
}

class Sensor {
  + Read() : float
}

MotorController --> Sensor : uses

@enduml
```

## Common Patterns for Embedded Systems

### Hardware + Firmware Layers
```plantuml
@startuml
!theme cerulean
title Layered Architecture

package "Hardware" {
  [Microcontroller]
  [Peripherals]
}

package "HAL" {
  [GPIO Driver]
  [Timer Driver]
}

package "Application" {
  [Business Logic]
  [State Machine]
}

[Business Logic] --> [GPIO Driver]
[State Machine] --> [Timer Driver]
[GPIO Driver] --> [Microcontroller]
[Timer Driver] --> [Microcontroller]

@enduml
```

### Interrupt-Driven Flow
```plantuml
@startuml
!theme cerulean

participant "Main Loop" as Main
participant "ISR Handler" as ISR
participant "Hardware" as HW

Main -> HW : Enable interrupt
activate Main

HW -> ISR : Interrupt triggered
activate ISR
ISR -> ISR : Process event
ISR -> Main : Set flag
deactivate ISR

Main -> Main : Check flag
Main -> Main : Handle event
deactivate Main

@enduml
```

### Ring Buffer
```plantuml
@startuml
!theme cerulean

queue "Index 0" as B0
queue "Index 1 (TAIL)" as B1 #LightGreen
queue "Index 2" as B2 #LightGreen
queue "Index 3 (HEAD)" as B3 #LightCoral
queue "Index 4" as B4

note right of B1
  TAIL = Read position
  HEAD = Write position
  Count = (HEAD - TAIL + SIZE) % SIZE
end note

@enduml
```

## Useful Styling

### Colors
```plantuml
component [Red Component] #Red
component [Green Component] #LightGreen
component [Blue Component] #LightBlue
component [Yellow Component] #Gold
component [Gray Component] #LightGray
```

### Arrows
```plantuml
A -> B : solid arrow
A --> B : dashed arrow
A -up-> B : arrow direction
A -[#Red]-> B : colored arrow
A -[#Red,dashed]-> B : colored dashed
A -[hidden]-> B : hidden (for layout)
```

### Notes
```plantuml
note right of Component
  This is a note
  on the right side
end note

note left of Component : Short note

note as N1
  Floating note
end note
```

### Themes
```plantuml
!theme cerulean    ' Professional blue
!theme plain       ' Simple black/white
!theme sketchy     ' Hand-drawn style
!theme amiga       ' Retro computer style
```

## Embedded-Specific Diagrams

### Timer Architecture
```plantuml
@startuml
!theme cerulean

component "Timer 1\n@ 1kHz" as T1
component "Timer 2\n(Variable)" as T2
component "PWM Output" as PWM
component "Hardware Pin" as Pin

T1 -down-> T2 : Configure period
T2 -down-> PWM : Time base
PWM -down-> Pin : Pulse output

note right of T1
  Control loop timer
  Updates every 1ms
end note

@enduml
```

### Memory Map
```plantuml
@startuml
!theme cerulean

rectangle "Flash (2MB)" {
  rectangle "Code" #LightBlue
  rectangle "Constants" #LightGreen
}

rectangle "RAM (512KB)" {
  rectangle "Stack (20KB)" #LightCoral
  rectangle "Heap (20KB)" #Gold
  rectangle "Globals" #LightGray
}

@enduml
```

### State Machine with Timing
```plantuml
@startuml
!theme cerulean

state "Init" as Init
state "Running" as Run
state "Error" as Error

[*] --> Init
Init --> Run : <1ms
Run --> Run : Loop\n(100ms)
Run --> Error : Fault detected
Error --> Init : Reset\n(10ms)
Error --> [*] : Critical\n(immediate)

@enduml
```

## Tips & Tricks

### 1. Layout Control
```plantuml
A -right-> B   ' Force direction (left, right, up, down)
A -[hidden]-> B  ' Hidden arrow for spacing
```

### 2. Grouping
```plantuml
package "Group Name" {
  [Component 1]
  [Component 2]
}

rectangle "Rectangle" {
  [Component 3]
}
```

### 3. Stereotypes
```plantuml
component [MyComponent] <<interface>>
component [MyDriver] <<hardware>>
component [MyAPI] <<library>>
```

### 4. Scale
```plantuml
@startuml
scale 1.5    ' Make diagram larger
scale 0.75   ' Make diagram smaller
@enduml
```

### 5. Line Breaks
```plantuml
component [Long Name\nWith Line Break]
note right
  Multi-line note
  Line 2
  Line 3
end note
```

## Export Commands

```bash
# PNG (raster image)
java -jar plantuml.jar -tpng diagram.puml

# SVG (scalable vector)
java -jar plantuml.jar -tsvg diagram.puml

# ASCII art (for documentation)
java -jar plantuml.jar -ttxt diagram.puml

# Multiple files
java -jar plantuml.jar *.puml
```

## VS Code Shortcuts

- `Alt + D` : Preview diagram
- `Ctrl + Shift + P` → "PlantUML: Export Current Diagram"
- `Ctrl + Shift + P` → "PlantUML: Export Workspace Diagrams"

## Common Mistakes

❌ **Wrong**: Forgetting `@startuml` / `@enduml`
✅ **Correct**: Always wrap diagram in these tags

❌ **Wrong**: Using spaces in identifiers without quotes
✅ **Correct**: Use [Component Name] or "Component Name" as CompName

❌ **Wrong**: Circular dependencies in component diagrams
✅ **Correct**: Layered architecture (no cycles)

❌ **Wrong**: Too much detail in one diagram
✅ **Correct**: Multiple focused diagrams (one concept each)

## Best Practices for Embedded Systems

1. **Start Simple**: High-level overview first, add detail incrementally
2. **Separate Concerns**: One diagram per concept (data flow, timing, dependencies)
3. **Use Layers**: Hardware → HAL → Application is natural for embedded
4. **Document Timing**: Sequence diagrams for interrupt flows are critical
5. **Show State**: State machines are everywhere in embedded systems
6. **Version Control**: Text files work great with Git
7. **Keep Updated**: Diagrams should match code (review in PRs)

## Resources

- **Official Docs**: https://plantuml.com/
- **Real Examples**: https://real-world-plantuml.com/
- **Online Editor**: https://www.plantuml.com/plantuml/uml/
- **VS Code Extension**: Search "PlantUML" by jebbs in Extensions
- **This Project**: See docs/plantuml/ for working examples

---

**Pro Tip**: Start every new embedded project by creating:
1. System overview diagram (hardware + firmware layers)
2. Data flow diagram (input → processing → output)
3. Module dependency diagram (what includes what)

These three diagrams will guide your entire architecture!
