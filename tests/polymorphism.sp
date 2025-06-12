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
    
    // Method that uses inherited fields with string concatenation
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
    
    // Method that uses both inherited and own fields with string concatenation
    public void ShowBirdInfo() {
        Mycelium_String_print("Bird Info:");
        Mycelium_String_print("Name: " + name);     // inherited from Animal
        Mycelium_String_print("Age: " + age);       // inherited from Animal
        Mycelium_String_print("Species: " + species);  // own field
        Mycelium_String_print("Can Fly: " + canFly);   // own field
    }
    
    // Method that modifies inherited fields
    public void SetBirdInfo(string newName, int newAge, string newSpecies, bool flies) {
        name = newName;        // inherited field
        age = newAge;          // inherited field  
        species = newSpecies;  // own field
        canFly = flies;        // own field
    }
    
    // Virtual method that demonstrates field access in inheritance
    public virtual void IntroduceSelf() {
        Mycelium_String_print("Hello, I am a bird named: " + name);
        Mycelium_String_print("I am a " + species);
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
    
    // Method that uses fields from all inheritance levels with string concatenation
    public void ShowEagleInfo() {
        Mycelium_String_print("Eagle Info:");
        Mycelium_String_print("Name: " + name);        // inherited from Animal (2 levels up)
        Mycelium_String_print("Age: " + age);          // inherited from Animal (2 levels up)
        Mycelium_String_print("Species: " + species);  // inherited from Bird (1 level up)
        Mycelium_String_print("Can Fly: " + canFly);   // inherited from Bird (1 level up)
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
        Mycelium_String_print("Hello, I am an eagle named: " + name);
        Mycelium_String_print("I am a " + species + " and I am mighty!");
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
    
    // Method that uses inherited fields with string concatenation
    public void ShowFishInfo() {
        Mycelium_String_print("Fish Info:");
        Mycelium_String_print("Name: " + name);     // inherited from Animal
        Mycelium_String_print("Age: " + age);       // inherited from Animal
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
        
        // Test 1: String concatenation with primitives
        Mycelium_String_print("\n--- Test 1: String + Primitive Concatenation ---");
        
        int testInt = 42;
        bool testBool = true;
        
        Mycelium_String_print("String + int: " + testInt);
        Mycelium_String_print("String + bool: " + testBool);
        Mycelium_String_print(testInt + " <- int + string");
        Mycelium_String_print(testBool + " <- bool + string");
        
        // Test 2: Basic inheritance - base class
        Mycelium_String_print("\n--- Test 2: Base Class (Animal) ---");
        Animal animal = new Animal();
        animal.SetBasicInfo("Buddy", 5);
        animal.ShowInfo();
        animal.MakeSound();     // Animal's virtual method
        animal.Move();          // Animal's virtual method  
        animal.Sleep();         // Animal's virtual method
        animal.Eat();           // Animal's non-virtual method
        animal.Breathe();       // Animal's non-virtual method
        
        // Test 3: Single-level inheritance - Bird inherits from Animal
        Mycelium_String_print("\n--- Test 3: Single-Level Inheritance (Bird : Animal) ---");
        Bird bird = new Bird();
        bird.SetBirdInfo("Tweety", 2, "Canary", true);
        bird.ShowBirdInfo();
        bird.MakeSound();       // Bird's override (should print "Tweet tweet!")
        bird.Move();            // Bird's override (should print "Bird flies...")
        bird.Sleep();           // Animal's inherited method (should print "Animal sleeps")
        bird.Eat();             // Animal's inherited non-virtual method
        bird.Breathe();         // Animal's inherited non-virtual method
        bird.BuildNest();       // Bird's new virtual method
        bird.Preen();           // Bird's new non-virtual method
        bird.IntroduceSelf();   // Bird's virtual method using inherited fields
        
        // Test 4: Multi-level inheritance - Eagle inherits from Bird
        Mycelium_String_print("\n--- Test 4: Multi-Level Inheritance (Eagle : Bird : Animal) ---");
        Eagle eagle = new Eagle();
        eagle.SetEagleInfo("Thunder", 7, "Golden Eagle", true, 200);
        eagle.ShowEagleInfo();
        eagle.MakeSound();      // Eagle's override (should print "Eagle screeches!")
        eagle.Move();           // Eagle's override (should print "Eagle soars...")
        eagle.Sleep();          // Animal's inherited method (should print "Animal sleeps")
        eagle.Eat();            // Animal's inherited non-virtual method
        eagle.Breathe();        // Animal's inherited non-virtual method  
        eagle.BuildNest();      // Eagle's override (should print "Eagle builds nest...")
        eagle.Preen();          // Bird's inherited non-virtual method
        eagle.Hunt();           // Eagle's new virtual method
        eagle.IntroduceSelf();  // Eagle's override that uses inherited fields
        
        // Test 5: Parallel inheritance - Fish inherits from Animal
        Mycelium_String_print("\n--- Test 5: Parallel Inheritance (Fish : Animal) ---");
        Fish fish = new Fish();
        fish.SetFishInfo("Nemo", 3, true);
        fish.ShowFishInfo();
        fish.MakeSound();       // Fish's override (should print "Fish makes bubbles")
        fish.Move();            // Animal's inherited method (should print "Animal moves")
        fish.Sleep();           // Fish's override (should print "Fish rests...")
        fish.Eat();             // Animal's inherited non-virtual method
        fish.Breathe();         // Animal's inherited non-virtual method
        fish.Swim();            // Fish's new virtual method
        
        // Test 6: Polymorphic assignment and virtual method dispatch
        Mycelium_String_print("\n--- Test 6: Polymorphic Assignment & Virtual Dispatch ---");
        
        // Polymorphic assignment: derived -> base
        Animal animalRef1 = bird;        // Bird -> Animal
        Animal animalRef2 = eagle;       // Eagle -> Animal (2-level upcast)
        Animal animalRef3 = fish;        // Fish -> Animal
        
        Mycelium_String_print("Polymorphic virtual method calls:");
        animalRef1.MakeSound();  // Should call Bird.MakeSound ("Tweet tweet!")
        animalRef2.MakeSound();  // Should call Eagle.MakeSound ("Eagle screeches!")
        animalRef3.MakeSound();  // Should call Fish.MakeSound ("Fish makes bubbles")
        
        // Polymorphic assignment with new expressions
        Animal polyAnimal1 = new Bird();   // Direct polymorphic assignment
        Animal polyAnimal2 = new Eagle();  // Direct polymorphic assignment
        Animal polyAnimal3 = new Fish();   // Direct polymorphic assignment
        
        polyAnimal1.SetBasicInfo("PolyBird", 4);
        polyAnimal2.SetBasicInfo("PolyEagle", 8);
        polyAnimal3.SetBasicInfo("PolyFish", 2);
        
        Mycelium_String_print("Polymorphic field access and method calls:");
        polyAnimal1.ShowInfo();  // Animal method using inherited fields
        polyAnimal1.MakeSound(); // Should call Bird.MakeSound
        
        polyAnimal2.ShowInfo();  // Animal method using inherited fields
        polyAnimal2.MakeSound(); // Should call Eagle.MakeSound
        
        polyAnimal3.ShowInfo();  // Animal method using inherited fields
        polyAnimal3.MakeSound(); // Should call Fish.MakeSound
        
        // Test 7: Bird-level polymorphism
        Mycelium_String_print("\n--- Test 7: Bird-Level Polymorphism ---");
        Bird birdRef = eagle;    // Eagle -> Bird (1-level upcast)
        
        birdRef.SetBirdInfo("PolyEagle2", 10, "Bald Eagle", true);
        birdRef.ShowBirdInfo();  // Bird method using inherited and own fields
        birdRef.MakeSound();     // Should call Eagle.MakeSound (virtual dispatch)
        birdRef.IntroduceSelf(); // Should call Eagle.IntroduceSelf (virtual dispatch)
        
        // Test 8: Direct field assignment across inheritance levels
        Mycelium_String_print("\n--- Test 8: Direct Field Assignment & Access ---");
        Eagle directEagle = new Eagle();
        directEagle.name = "Storm";         // inherited from Animal (2 levels)
        directEagle.age = 10;               // inherited from Animal (2 levels)
        directEagle.species = "Bald Eagle"; // inherited from Bird (1 level)
        directEagle.canFly = true;          // inherited from Bird (1 level)
        directEagle.wingspan = 250;         // own field
        directEagle.ShowEagleInfo();        // Should show all field values with string concatenation
        
        // Test 9: Complex string concatenation scenarios
        Mycelium_String_print("\n--- Test 9: Complex String Concatenation ---");
        
        string prefix = "Value is: ";
        int number = 123;
        bool flag = false;
        
        Mycelium_String_print(prefix + number);                    // string + int
        Mycelium_String_print(prefix + flag);                      // string + bool
        Mycelium_String_print(number + " is the number");          // int + string
        Mycelium_String_print(flag + " is the flag value");        // bool + string
        Mycelium_String_print("Complex: " + number + " and " + flag); // multiple concatenations
        
        Mycelium_String_print("\n=== ALL INHERITANCE & POLYMORPHISM TESTS COMPLETE ===");
        return 0;
    }
}