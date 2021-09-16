﻿using System.Text;

namespace AsmGen
{
    public class Fadd256SchedTest : UarchTest
    {
        public string Prefix { get => "fadd256sched"; }
        public string Description { get => "256-bit FADD Scheduler Capacity Test - x86 only"; }
        public string FunctionDefinitionParameters { get => "uint64_t iterations, int *arr, float *floatArr"; }
        public string GetFunctionCallParameters { get => "structIterations, A, fpArr"; }

        public int[] Counts { get; private set; }
        public bool DivideTimeByCount => false;

        public Fadd256SchedTest(int low, int high, int step)
        {
            this.Counts = UarchTestHelpers.GenerateCountArray(low, high, step);
        }

        public void GenerateX86GccAsm(StringBuilder sb)
        {
            // ymm0 is dependent on ptr chasing load
            string[] unrolledAdds = new string[4];
            unrolledAdds[0] = "  vaddps %ymm0, %ymm1, %ymm1";
            unrolledAdds[1] = "  vaddps %ymm0, %ymm2, %ymm2";
            unrolledAdds[2] = "  vaddps %ymm0, %ymm3, %ymm3";
            unrolledAdds[3] = "  vaddps %ymm0, %ymm4, %ymm3";

            UarchTestHelpers.GenerateX86AsmFp256SchedTestFuncs(sb, this.Counts, this.Prefix, unrolledAdds, unrolledAdds);
        }

        public void GenerateX86NasmAsm(StringBuilder sb)
        {
            string[] unrolledAdds = new string[4];
            unrolledAdds[0] = "  vaddps ymm1, ymm1, ymm0";
            unrolledAdds[1] = "  vaddps ymm2, ymm2, ymm0";
            unrolledAdds[2] = "  vaddps ymm3, ymm3, ymm0";
            unrolledAdds[3] = "  vaddps ymm4, ymm4, ymm0";
            UarchTestHelpers.GenerateX86NasmFp256SchedTestFuncs(sb, this.Counts, this.Prefix, unrolledAdds, unrolledAdds);
        }

        public void GenerateArmAsm(StringBuilder sb)
        {
            string[] unrolledAdds = new string[4];
            unrolledAdds[0] = "  fadd s17, s17, s16";
            unrolledAdds[1] = "  fadd s18, s18, s16";
            unrolledAdds[2] = "  fadd s19, s19, s16";
            unrolledAdds[3] = "  fadd s20, s20, s16";
            UarchTestHelpers.GenerateArmAsmFpSchedTestFuncs(sb, this.Counts, this.Prefix, unrolledAdds, unrolledAdds);
        }
    }
}