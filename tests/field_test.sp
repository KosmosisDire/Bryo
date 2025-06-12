extern void Mycelium_String_print(string str);

class Animal {
    public string name;
    public int age;
    
    public Animal() {
    }
}

class Bird : Animal {
    public string species;
    
    public Bird() {
    }
}

class Program {
    static int Main()
    {
        Mycelium_String_print("=== FIELD INHERITANCE TEST ===");
        
        // Test 1: Base class field access
        Animal animal = new Animal();
        // animal.name = "Generic Animal";  // Assignment test
        // animal.age = 5;                  // Assignment test
        
        // Test 2: Inherited field access
        Bird bird = new Bird();
        bird.name = "Tweety";      // Should access Animal.name field
        bird.age = 2;              // Should access Animal.age field  
        bird.species = "Canary";   // Should access Bird.species field
        
        Mycelium_String_print("Field inheritance test complete");
        return 0;
    }
}