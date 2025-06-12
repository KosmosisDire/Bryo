extern void Mycelium_String_print(string str);

// Base class with virtual and non-virtual methods
class Animal {
    public string name;
    public int age;
    
    public Animal() {
    }
    
    public virtual void MakeSound() {
        Mycelium_String_print("Generic animal sound");
    }
    
    public virtual void Move() {
        Mycelium_String_print("Animal moves");
    }
    
    public virtual void Sleep() {
        Mycelium_String_print("Animal sleeps");
    }
    
    public void Eat() {
        Mycelium_String_print("Animal eats (non-virtual)");
    }
    
    public void Breathe() {
        Mycelium_String_print("Animal breathes (non-virtual)");
    }
    
    // Method that uses inherited fields
    public void ShowInfo() {
        Mycelium_String_print("Animal Info:");
        Mycelium_String_print("Name: " + name);
        Mycelium_String_print("Age: " + age);
    }
    
    // Method that modifies inherited fields
    public void SetBasicInfo(string newName, int newAge) {
        name = newName;
        age = newAge;
    }
}

// First level inheritance - Bird inherits from Animal
class Bird : Animal {
    public string species;
    public bool canFly;
    
    public Bird() {
    }
    
    // Override base virtual method
    public virtual void MakeSound() {
        Mycelium_String_print("Tweet tweet!");
    }
    
    // Override another base virtual method  
    public virtual void Move() {
        Mycelium_String_print("Bird flies through the air");
    }
    
    // New virtual method specific to Bird
    public virtual void BuildNest() {
        Mycelium_String_print("Bird builds a nest");
    }
    
    // New non-virtual method
    public void Preen() {
        Mycelium_String_print("Bird preens its feathers");
    }
    
    // Method that uses both inherited and own fields
    public void ShowBirdInfo() {
        Mycelium_String_print("Bird Info:");
        Mycelium_String_print("Name: " + name);     // inherited from Animal
        Mycelium_String_print("Age: " + age);  // inherited from Animal
        Mycelium_String_print("Species: " + species);  // own field
        Mycelium_String_print("Can Fly: " + canFly); // own field
    }
    
    // Method that modifies inherited fields
    public void SetBirdInfo(string newName, int newAge, string newSpecies, bool flies) {
        name = newName;        // inherited field
        age = newAge;          // inherited field  
        species = newSpecies;  // own field
        canFly = flies;        // own field
    }
    
    // Method that demonstrates field access in virtual method
    public virtual void IntroduceSelf() {
        Mycelium_String_print("Hello, I am a bird named:");
        Mycelium_String_print(name);
        Mycelium_String_print("I am a:");
        Mycelium_String_print(species);
    }
}

// Second level inheritance - Eagle inherits from Bird
class Eagle : Bird {
    public int wingspan;
    
    public Eagle() {
    }
    
    // Override Bird's virtual method
    public virtual void MakeSound() {
        Mycelium_String_print("Eagle screeches!");
    }
    
    // Override Bird's Move method
    public virtual void Move() {
        Mycelium_String_print("Eagle soars majestically");
    }
    
    // Override Bird's BuildNest method
    public virtual void BuildNest() {
        Mycelium_String_print("Eagle builds nest on high cliff");
    }
    
    // New virtual method specific to Eagle
    public virtual void Hunt() {
        Mycelium_String_print("Eagle hunts for prey");
    }
    
    // Method that uses fields from all inheritance levels
    public void ShowEagleInfo() {
        Mycelium_String_print("Eagle Info:");
        Mycelium_String_print("Name: " + name);     // inherited from Animal (2 levels up)
        Mycelium_String_print("Age: " + age);  // inherited from Animal (2 levels up)
        Mycelium_String_print("Species: " + species);  // inherited from Bird (1 level up)
        Mycelium_String_print("Can Fly: " + canFly); // inherited from Bird (1 level up)
        Mycelium_String_print("Wingspan: " + wingspan); // own field
    }
    
    // Method that modifies fields from all inheritance levels
    public void SetEagleInfo(string newName, int newAge, string newSpecies, bool flies, int newWingspan) {
        name = newName;        // inherited from Animal
        age = newAge;          // inherited from Animal
        species = newSpecies;  // inherited from Bird  
        canFly = flies;        // inherited from Bird
        wingspan = newWingspan; // own field
    }
    
    // Override virtual method that uses inherited fields
    public virtual void IntroduceSelf() {
        Mycelium_String_print("Hello, I am an eagle named:");
        Mycelium_String_print(name);     // inherited from Animal
        Mycelium_String_print("I am a:");
        Mycelium_String_print(species);  // inherited from Bird
        Mycelium_String_print("And I am mighty!");
    }
}

// Parallel inheritance - Fish inherits from Animal
class Fish : Animal {
    public bool isSaltwater;
    
    public Fish() {
    }
    
    // Override some base methods, leave others inherited
    public virtual void MakeSound() {
        Mycelium_String_print("Fish makes bubbles");
    }
    
    // Leave Move() inherited from Animal
    
    // Override Sleep
    public virtual void Sleep() {
        Mycelium_String_print("Fish rests near coral");
    }
    
    // New virtual method
    public virtual void Swim() {
        Mycelium_String_print("Fish swims gracefully");
    }
    
    // Method that uses inherited fields
    public void ShowFishInfo() {
        Mycelium_String_print("Fish Info:");
        Mycelium_String_print("Name: " + name);     // inherited from Animal
        Mycelium_String_print("Age: " + age);  // inherited from Animal
        Mycelium_String_print("Is Saltwater: " + isSaltwater); // own field
    }
    
    // Method that modifies inherited fields
    public void SetFishInfo(string newName, int newAge, bool saltwater) {
        name = newName;        // inherited field
        age = newAge;          // inherited field
        isSaltwater = saltwater; // own field
    }
}

class Program {
    static int Main()
    {
        Mycelium_String_print("=== COMPREHENSIVE INHERITANCE & POLYMORPHISM TEST ===");
        
        // Test 1: Basic inheritance - base class
        Mycelium_String_print("\n--- Test 1: Base Class (Animal) ---");
        Animal animal = new Animal();
        animal.MakeSound();     // Animal's virtual method
        animal.Move();          // Animal's virtual method  
        animal.Sleep();         // Animal's virtual method
        animal.Eat();           // Animal's non-virtual method
        animal.Breathe();       // Animal's non-virtual method
        
        // Test 2: Single-level inheritance - Bird inherits from Animal
        Mycelium_String_print("\n--- Test 2: Single-Level Inheritance (Bird : Animal) ---");
        Bird bird = new Bird();
        bird.MakeSound();       // Bird's override (should print "Tweet tweet!")
        bird.Move();            // Bird's override (should print "Bird flies...")
        bird.Sleep();           // Animal's inherited method (should print "Animal sleeps")
        bird.Eat();             // Animal's inherited non-virtual method
        bird.Breathe();         // Animal's inherited non-virtual method
        bird.BuildNest();       // Bird's new virtual method
        bird.Preen();           // Bird's new non-virtual method
        
        // Test 3: Multi-level inheritance - Eagle inherits from Bird
        Mycelium_String_print("\n--- Test 3: Multi-Level Inheritance (Eagle : Bird : Animal) ---");
        Eagle eagle = new Eagle();
        eagle.MakeSound();      // Eagle's override (should print "Eagle screeches!")
        eagle.Move();           // Eagle's override (should print "Eagle soars...")
        eagle.Sleep();          // Animal's inherited method (should print "Animal sleeps")
        eagle.Eat();            // Animal's inherited non-virtual method
        eagle.Breathe();        // Animal's inherited non-virtual method  
        eagle.BuildNest();      // Eagle's override (should print "Eagle builds nest...")
        eagle.Preen();          // Bird's inherited non-virtual method
        eagle.Hunt();           // Eagle's new virtual method
        
        // Test 4: Parallel inheritance - Fish inherits from Animal
        Mycelium_String_print("\n--- Test 4: Parallel Inheritance (Fish : Animal) ---");
        Fish fish = new Fish();
        fish.MakeSound();       // Fish's override (should print "Fish makes bubbles")
        fish.Move();            // Animal's inherited method (should print "Animal moves")
        fish.Sleep();           // Fish's override (should print "Fish rests...")
        fish.Eat();             // Animal's inherited non-virtual method
        fish.Breathe();         // Animal's inherited non-virtual method
        fish.Swim();            // Fish's new virtual method
        
        // Test 5: Method resolution - each class should find correct methods
        Mycelium_String_print("\n--- Test 5: Method Resolution Verification ---");
        Mycelium_String_print("Testing that each class correctly resolves inherited vs overridden methods:");
        
        Mycelium_String_print("\nAnimal methods:");
        animal.MakeSound();     // Should be Animal's version
        
        Mycelium_String_print("\nBird methods:");  
        bird.MakeSound();       // Should be Bird's override
        bird.Sleep();           // Should be Animal's inherited version
        
        Mycelium_String_print("\nEagle methods:");
        eagle.MakeSound();      // Should be Eagle's override  
        eagle.Sleep();          // Should be Animal's inherited version (through Bird)
        eagle.Preen();          // Should be Bird's inherited version
        
        Mycelium_String_print("\nFish methods:");
        fish.MakeSound();       // Should be Fish's override
        fish.Move();            // Should be Animal's inherited version
        
        // Test 6: Virtual method dispatch verification
        Mycelium_String_print("\n--- Test 6: Virtual Method Dispatch Verification ---");
        Mycelium_String_print("All method calls should use virtual dispatch for virtual methods:");
        
        // Each object should use its own VTable
        animal.MakeSound();     // Animal VTable -> Animal.MakeSound
        bird.MakeSound();       // Bird VTable -> Bird.MakeSound  
        eagle.MakeSound();      // Eagle VTable -> Eagle.MakeSound
        fish.MakeSound();       // Fish VTable -> Fish.MakeSound
        
        // Test inherited virtual methods
        Mycelium_String_print("\nTesting inherited virtual methods:");
        bird.Sleep();           // Bird VTable -> Animal.Sleep
        eagle.Sleep();          // Eagle VTable -> Animal.Sleep  
        fish.Move();            // Fish VTable -> Animal.Move
        
        // Test 7: VTable layout verification  
        Mycelium_String_print("\n--- Test 7: VTable Layout Verification ---");
        Mycelium_String_print("Testing that VTable inheritance preserves method order:");
        
        // Animal VTable: [MakeSound, Move, Sleep]
        Mycelium_String_print("\nAnimal VTable order test:");
        animal.MakeSound();     // Index 0
        animal.Move();          // Index 1  
        animal.Sleep();         // Index 2
        
        // Bird VTable: [Bird.MakeSound, Bird.Move, Animal.Sleep, Bird.BuildNest]
        Mycelium_String_print("\nBird VTable order test:");
        bird.MakeSound();       // Index 0 (override)
        bird.Move();            // Index 1 (override)
        bird.Sleep();           // Index 2 (inherited)
        bird.BuildNest();       // Index 3 (new)
        
        // Eagle VTable: [Eagle.MakeSound, Eagle.Move, Animal.Sleep, Eagle.BuildNest, Eagle.Hunt]
        Mycelium_String_print("\nEagle VTable order test:");
        eagle.MakeSound();      // Index 0 (override)
        eagle.Move();           // Index 1 (override)  
        eagle.Sleep();          // Index 2 (inherited from Animal)
        eagle.BuildNest();      // Index 3 (override from Bird)
        eagle.Hunt();           // Index 4 (new)
        
        // Test 8: Field Inheritance Tests
        Mycelium_String_print("\n--- Test 8: Field Inheritance Tests ---");
        
        // Test 8a: Base class field access and assignment
        Mycelium_String_print("\n8a: Base Class Field Access & Assignment");
        Animal testAnimal = new Animal();
        testAnimal.SetBasicInfo("Buddy", 5);
        testAnimal.ShowInfo();
        
        // Test 8b: Single-level inheritance field access
        Mycelium_String_print("\n8b: Single-Level Inheritance Field Access (Bird)");
        Bird testBird = new Bird();
        testBird.SetBirdInfo("Tweety", 2, "Canary", true);
        testBird.ShowBirdInfo();
        testBird.IntroduceSelf();
        
        // Test 8c: Multi-level inheritance field access  
        Mycelium_String_print("\n8c: Multi-Level Inheritance Field Access (Eagle)");
        Eagle testEagle = new Eagle();
        testEagle.SetEagleInfo("Thunder", 7, "Golden Eagle", true, 200);
        testEagle.ShowEagleInfo();
        testEagle.IntroduceSelf(); // Virtual method that uses inherited fields
        
        // Test 8d: Parallel inheritance field access
        Mycelium_String_print("\n8d: Parallel Inheritance Field Access (Fish)");
        Fish testFish = new Fish();
        testFish.SetFishInfo("Nemo", 3, true);
        testFish.ShowFishInfo();
        
        // Test 8e: Direct field assignment and access
        Mycelium_String_print("\n8e: Direct Field Assignment & Access");
        Eagle directEagle = new Eagle();
        directEagle.name = "Storm";      // inherited from Animal (2 levels)
        directEagle.age = 10;            // inherited from Animal (2 levels)
        directEagle.species = "Bald Eagle"; // inherited from Bird (1 level)
        directEagle.canFly = true;       // inherited from Bird (1 level)
        directEagle.wingspan = 250;      // own field
        directEagle.ShowEagleInfo();
        
        // Test 8f: Field access in virtual method calls
        Mycelium_String_print("\n8f: Field Access in Virtual Method Calls");
        Bird polyBird = new Eagle();     // Polymorphic assignment
        polyBird.name = "Mighty";
        polyBird.species = "Harpy Eagle";
        polyBird.IntroduceSelf();        // Should call Eagle's override that uses inherited fields
        
        // Test 8g: Field assignment through inheritance chain
        Mycelium_String_print("\n8g: Field Assignment Through Inheritance Chain");
        Animal polyAnimal = new Eagle(); // Polymorphic assignment to base
        polyAnimal.name = "Zeus";
        polyAnimal.age = 15;
        polyAnimal.ShowInfo();           // Base class method using inherited fields
        
        Mycelium_String_print("\n=== ALL INHERITANCE & POLYMORPHISM TESTS COMPLETE ===");
        return 0;
    }
}