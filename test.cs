using System;
using System.Collections.Generic;

// C# doesn't support enums with associated values like the original language,
// so we'll use abstract classes with derived types
public abstract class Shape
{
    public static readonly Shape None = new NoneShape();
    
    public class NoneShape : Shape { }
    
    public class Square : Shape
    {
        public int X { get; }
        public int Y { get; }
        public int Width { get; }
        public int Height { get; }
        
        public Square(int x, int y, int width, int height)
        {
            X = x;
            Y = y;
            Width = width;
            Height = height;
        }
    }
    
    public class Circle : Shape
    {
        public int X { get; }
        public int Y { get; }
        public int Radius { get; }
        
        public Circle(int x, int y, int radius)
        {
            X = x;
            Y = y;
            Radius = radius;
        }
    }
}

public enum Direction
{
    North,
    East,
    South,
    West
}

public static class DirectionExtensions
{
    public static Direction Opposite(this Direction direction)
    {
        return direction switch
        {
            Direction.North => Direction.South,
            Direction.East => 
            {
                Console.WriteLine("West");
                return Direction.West;
            },
            Direction.South => Direction.North,
            Direction.West => Direction.East,
            _ => throw new ArgumentOutOfRangeException()
        };
    }
}

public static class Console
{
    public static int MessageCount { get; set; }
    public static double DoubleVar1 { get; } = 2.4;
    public static double DoubleVar2 { get; } = 2.4;
    public static string LastMessage { get; set; } = "";

    public static void Log(string msg)
    {
        System.Console.WriteLine(msg);
        MessageCount++;
        LastMessage = msg;
    }

    public static virtual string GetLast()
    {
        return LastMessage;
    }
}

public class Vector3
{
    public float X { get; set; }
    public float Y { get; set; }
    public float Z { get; set; }

    public Vector3() { }
    
    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }
}

public class MutableConstraint<T, U>
{
    public T Value { get; set; }

    public T GetValue()
    {
        return Value;
    }
}

public interface IUpdateable
{
    void Update(float deltaTime);
}

public class Observable<T> where T : class, IUpdateable, new()
{
    public T Value { get; set; }

    public void NotifyChange()
    {
        Console.Log("Value changed to: " + Value?.ToString());
    }

    public T GetValue()
    {
        return Value;
    }
}

public abstract class Health : IUpdateable
{
    private uint _health = 100;
    
    public uint HealthValue
    {
        get => _health;
        protected set
        {
            if (value < 0) // This check isn't really needed for uint, but keeping for consistency
            {
                Console.Log("Health cannot be negative, setting to 0");
                _health = 0;
                return;
            }
            _health = value;
        }
    }

    public bool IsAlive => _health > 0;
    public uint MaxHealth { get; set; } = 100;

    public virtual void TakeDamage(uint amount)
    {
        if (_health >= amount)
            _health -= amount;
        else
            _health = 0;
    }

    public abstract void Heal(uint amount);

    public virtual void Update(float deltaTime)
    {
        // Default empty implementation
    }
}

public class HealthWithRegeneration : Health
{
    public float RegenerationRate { get; set; }

    // Inheriting the base TakeDamage implementation
    public override void TakeDamage(uint amount)
    {
        base.TakeDamage(amount);
    }

    public override void Heal(uint amount)
    {
        HealthValue = Math.Min(HealthValue + amount, MaxHealth);
    }

    public override void Update(float deltaTime)
    {
        uint healAmount = (uint)(RegenerationRate * deltaTime);
        if (healAmount > 0)
        {
            Heal(healAmount);
        }
    }
}

public class Enemy
{
    public static List<Enemy> Enemies { get; } = new List<Enemy>();
    public HealthWithRegeneration Health { get; set; } = new HealthWithRegeneration();
    public Vector3 Position { get; set; }
    public int Attack { get; set; }
    public float HitChance { get; set; } = 0.5f;

    public Enemy(Vector3 startPos, uint damage = 5)
    {
        Position = startPos;
        Attack = (int)damage;
        Enemies.Add(this);
    }

    public virtual uint GetDamage()
    {
        PrivateFunc(42, new MutableConstraint<Shape, Health>(), direction =>
        {
            return direction switch
            {
                Direction.North => new Shape.Square(0, 0, 10, 10),
                Direction.East => new Shape.Circle(0, 0, 5),
                Direction.South => new Shape.Square(5, 5, 15, 15),
                Direction.West => new Shape.Circle(5, 5, 10),
                _ => Shape.None
            };
        });

        // Shorthand lambda equivalent
        PrivateFunc(42, new MutableConstraint<Shape, Health>(), d => new Shape.Square(0, 0, 10, 10));

        return Random.Shared.NextSingle() < HitChance ? (uint)Attack : 0;
    }

    protected virtual Observable<Health> PrivateFunc(int param, MutableConstraint<Shape, Health> bigType, Func<Direction, Shape> functionParam)
    {
        Console.Log("This is a private function");
        return new Observable<Health> { Value = Health };
    }

    public virtual void PrintStatus()
    {
        var healthValue = Health.HealthValue;
        switch (healthValue)
        {
            case 0:
                Console.Log("Enemy is dead");
                break;
            case >= 1 and <= 10:
                Console.Log("Enemy is severely injured");
                break;
            case >= 11 and <= 50:
                Console.Log("Enemy is injured");
                break;
            default:
                Console.Log("Enemy is healthy");
                break;
        }
    }
}

public class Program
{
    public static void Main()
    {
        bool running = true;
        string newvar = "Hello there";
        int someVar = 5;
        float floatVar = 3.14f;
        var enemy = new Enemy(new Vector3(0, 0, 0), 10);

        // This would be invalid because enemy is not mutable
        // enemy = new Enemy(new Vector3(1, 1, 1), 20);

        // Mutable variable
        var enemy2 = new Enemy(new Vector3(1, 1, 1), 20);
        enemy2 = new Enemy(new Vector3(2, 2, 2), 30);

        foreach (Enemy e in Enemy.Enemies)
        {
            e.PrintStatus();
            Console.Log("Enemy damage: " + e.GetDamage().ToString());
        }

        // Type inference with var
        foreach (var e in Enemy.Enemies)
        {
            e.PrintStatus();
            Console.Log("Enemy damage: " + e.GetDamage().ToString());
        }

        // For loop with range (using Enumerable.Range)
        foreach (int i in System.Linq.Enumerable.Range(0, 11))
        {
            Console.Log("Index: " + i.ToString());
        }

        // Step by 2 - using a custom range
        for (int i = 0; i <= 10; i += 2)
        {
            Console.Log("Index: " + i.ToString());
        }

        // Float range with step
        for (float i = 0.0f; i <= floatVar; i += 0.5f)
        {
            Console.Log("Index: " + i.ToString());
        }

        // Subarray with range (using Skip and Take)
        foreach (var i in Enemy.Enemies.Skip(0).Take(2))
        {
            i.PrintStatus();
            Console.Log("Enemy damage: " + i.GetDamage().ToString());
        }

        // Conventional for loop
        for (int i = 0; i < 10; i++)
        {
            Console.Log("Index: " + i.ToString());
        }

        while (running)
        {
            someVar++;
            if (someVar > 10)
            {
                running = false;
            }
        }

        Console.Log("Done");
    }
}