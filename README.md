<div align="center">
  <img src="https://docs.fern-lang.org/fern-icon.svg" width="64" />
  <h1>The Fern Programming Language</h1>
</div>

Fern is a general, object oriented, statically typed, embeddable language inspired by C#, Rust, Swift, and Python.
It aims to be intuitive, performant, safe, powerful, and bring tooling along with it for easy usage. It's intended purpose is for games programming for modding, hot reloading, quick iteration, and code glue without compromise.

> [!WARNING]
> The Fern compiler is in early development, the compiler does work and is able to run JIT-compiled scripts using LLVM, but is not ready for any serious usage.


## Quick Example

```lua
type Person
{
    string name
    i32 age

    fn HaveBirthday
    {
        age += 1
        Print("Happy birthday {name}! You're now {age}")
    }
}

var alice = new Person("Alice", 30)
alice.HaveBirthday()  -- "Happy birthday Alice! You're now 31"
```

## Documentation

I made a simple website with an overview of the syntax of Fern:
[docs.fern-lang.org](https://docs.fern-lang.org/)

Not everything shown there is implemented yet, but it shows the spec I am using when developing the language.
