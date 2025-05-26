// tests/test_casting.sp

class Program {
    static int Main() {
        // --- Integer to Integer ---
        int i_val1 = 10;
        long l_val1 = (long)i_val1;
        short s_val1 = (short)i_val1;
        char c_val1 = (char)i_val1;

        long l_val2 = 123456789012L;
        int i_val2 = (int)l_val2;

        int i_val3 = -5;
        short s_val2 = (short)i_val3;

        // --- Float to Float ---
        float f_val1 = 3.14f;
        double d_val1 = (double)f_val1;

        double d_val2 = 2.718281828459045;
        float f_val2 = (float)d_val2;

        // --- Integer to Float ---
        int i_val4 = 100;
        float f_val3 = (float)i_val4;
        double d_val3 = (double)i_val4;

        long l_val3 = -50L;
        float f_val4 = (float)l_val3;

        // --- Float to Integer ---
        float f_val5 = 123.75f;
        int i_val5 = (int)f_val5;

        double d_val4 = -456.25;
        int i_val6 = (int)d_val4;

        float f_val6 = 987.999f;
        long l_val4 = (long)f_val6;

        // --- Boolean to Integer ---
        bool b_true = true;
        bool b_false = false;
        int int_from_true = (int)b_true;
        int int_from_false = (int)b_false;

        // --- Integer to Boolean ---
        int int_zero = 0;
        int int_one = 1;
        int int_neg_one = -1;
        bool bool_from_zero = (bool)int_zero;
        bool bool_from_one = (bool)int_one;
        bool bool_from_neg_one = (bool)int_neg_one;

        // --- No-op Cast ---
        int i_val7 = 77;
        int i_val8 = (int)i_val7;

        // --- Char related (char is i8) ---
        char c_val_A = (char)65; // int to char
        // int i_val_char = (int)'B'; // Assuming char literal 'B' might not be supported yet

        // Use the variables to prevent them from being optimized away entirely
        // This is a simple way to ensure the cast operations are generated.
        if (l_val1 > 0 && s_val1 > 0 && c_val1 > 0 && i_val2 != 0 && s_val2 < 0 &&
            d_val1 > 0.0 && f_val2 > 0.0 && f_val3 > 0.0 && d_val3 > 0.0 && f_val4 < 0.0 &&
            i_val5 > 0 && i_val6 < 0 && l_val4 > 0 &&
            int_from_true == 1 && int_from_false == 0 &&
            bool_from_zero == false && bool_from_one == true && bool_from_neg_one == true &&
            i_val8 == 77 && c_val_A == 65) {
            return 1; // All conditions met
        }

        return 0; // Some condition not met or default return
    }
}
