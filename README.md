<img width="250" alt="bryo-full-planet@4x" src="https://github.com/user-attachments/assets/5f68aa97-2d24-4ad2-8ee8-1b2faed55694" />

# The Bryo Programming Language
Bryo, (name from `Bryophyta` or `Bryo` meaning moss) is a general, object oriented, statically typed, embeddable language inspired by C#, Rust, Swift, and Python.
It aims to be intuitive, performant, safe, powerful, and bring tooling along with it for easy usage. It's intended purpose is for games programming for modding, hot reloading, quick iteration, and code glue without compromise.

It is currently a work in progress, but the compiler does work and is able to run JIT-compiled scripts using LLVM.


## Syntax Examples

### Comments

```lua
-- this is a comment

---
This is a fantastic
multiline comment!
---
```

### Variables

```lua
i32 x = 10;        -- semicolons are optional
var y = x + 10     -- type is inferred
```

### Functions

```lua
fn TestFunction: i32 -- parentheses () on a function are optional if the function has no arguments
{
    return 42
}

fn InferredReturn(i32 input) -- return type is inferred
{
    -- do stuff
    return TestFunction()
}
```

### Types

```lua
type Point
{
    f32 x, y

    -- member functions work as you'd expect
    fn Move(f32 byX, f32 byY)
    {
        x += byX
        y += byY
    }

    var SqrMagnitude => x * x + y * y -- Arrow properties act like variables but they return calcualted read only data (it is caluclated every time it is read)

    -- you can also make getter and setter properties (this shows a setter only property. It cannot be read from only written to)
    f32[] vector
    {
        set
        {
            x = value[0]
            y = value[1]
        }
    }
}

fn Main
{
    var p = new Point()
    p.vector = [10.0, 5.0]
    
    Print("Hello World")

    return 0.0
}
```

### Loops

```lua

for (var i = 0; i < 10; i += 1)
{
    -- do stuff
}

var x = 0
while (x < 10)
{
    -- do stuff
    x += 1
}

-- In the future this for loop syntax will be added
for (5) -- run a loop 5 times
for (var i in ..5) -- run a loop 5 times with an iterator variable
for (var i in 10..20 by 2) -- run a loop from 10 - 20 jumping by twos (10, 12, 14, 16, 18, 20) 
for (var item in iterable) -- loop through items in an iterable (list, array, etc)
for (var item in iterable at var i) -- loop through items in an iterable and track the index too
```


