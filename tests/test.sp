extern void Mycelium_String_print(string str);
extern void print_int(int val);

namespace ScopeTest
{
    // Test self-referencing binary tree node
    class TreeNode
    {
        int value;
        TreeNode left;
        TreeNode right;
        
        TreeNode(int val)
        {
            value = val;
            left = null;
            right = null;
        }
        
        void set_left(TreeNode node)
        {
            left = node;
        }
        
        void set_right(TreeNode node)
        {
            right = node;
        }
        
        int get_value()
        {
            return value;
        }
        
        // Recursive tree traversal (in-order)
        void print_inorder()
        {
            if (left != null)
                left.print_inorder();
            
            print_int(value);
            
            if (right != null)
                right.print_inorder();
        }
        
        // Recursive tree depth calculation
        int get_depth()
        {
            int left_depth = 0;
            int right_depth = 0;
            
            if (left != null)
                left_depth = left.get_depth();
            
            if (right != null)
                right_depth = right.get_depth();
            
            if (left_depth > right_depth)
                return left_depth + 1;
            else
                return right_depth + 1;
        }
    }
    
    // Test simple linked list node
    class ListNode
    {
        int data;
        ListNode next;
        
        ListNode(int val)
        {
            data = val;
            next = null;
        }
        
        void set_next(ListNode node)
        {
            next = node;
        }
        
        // Recursive list printing
        void print_list()
        {
            print_int(data);
            
            if (next != null)
                next.print_list();
        }
        
        // Recursive list length calculation
        int get_length()
        {
            if (next == null)
                return 1;
            else
                return 1 + next.get_length();
        }
    }

    class Program
    {
        // Test recursive fibonacci
        static int fib(int n)
        {
            if (n <= 1)
                return n;
            return fib(n - 1) + fib(n - 2);
        }
        
        // Test recursive factorial
        static int factorial(int n)
        {
            if (n <= 1)
                return 1;
            return n * factorial(n - 1);
        }
        
        // Test mutual recursion helper
        static bool is_even(int n)
        {
            if (n == 0)
                return true;
            return is_odd(n - 1);
        }
        
        static bool is_odd(int n)
        {
            if (n == 0)
                return false;
            return is_even(n - 1);
        }

        static int Main()
        {
            print_int(999); // Test marker start
            
            // Test 1: Recursive functions
            print_int(fib(6));        // Should print 8
            print_int(factorial(5));  // Should print 120
            
            // Test 2: Mutual recursion
            if (is_even(4))
                print_int(1); // Should print 1 (true)
            else
                print_int(0);
                
            if (is_odd(4))
                print_int(1);
            else
                print_int(0); // Should print 0 (false)
            
            // Test 3: Self-referencing binary tree
            TreeNode root = new TreeNode(5);
            TreeNode left_child = new TreeNode(3);
            TreeNode right_child = new TreeNode(7);
            TreeNode left_left = new TreeNode(1);
            TreeNode left_right = new TreeNode(4);
            
            root.set_left(left_child);
            root.set_right(right_child);
            left_child.set_left(left_left);
            left_child.set_right(left_right);
            
            // Tree should print: 1 3 4 5 7 (in-order traversal)
            root.print_inorder();
            
            // Tree depth should be 3
            print_int(root.get_depth());
            
            // Test 4: Self-referencing linked list
            ListNode head = new ListNode(10);
            ListNode second = new ListNode(20);
            ListNode third = new ListNode(30);
            
            head.set_next(second);
            second.set_next(third);
            
            // List should print: 10 20 30
            head.print_list();
            
            // List length should be 3
            print_int(head.get_length());
            
            print_int(888); // Test marker end
            
            return 0;
        }
    }
}