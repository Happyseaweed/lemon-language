; ModuleID = 'LEMON JIT'
source_filename = "LEMON JIT"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"

@p = global double 0.000000e+00
@a = global double 0.000000e+00
@b = global double 0.000000e+00
@bigChungus = global double 0.000000e+00
@sumTest = global double 0.000000e+00
@llvm.global_ctors = appending global [5 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 0, ptr @_init_global_p, ptr null }, { i32, ptr, ptr } { i32 1, ptr @_init_global_a, ptr null }, { i32, ptr, ptr } { i32 2, ptr @_init_global_b, ptr null }, { i32, ptr, ptr } { i32 3, ptr @_init_global_bigChungus, ptr null }, { i32, ptr, ptr } { i32 4, ptr @_init_global_sumTest, ptr null }]

define double @_main() {
entry:
  %calltmp = call double @test()
  %calltmp1 = call double @another(double 9.000000e+00)
  %addtmp = fadd double %calltmp, %calltmp1
  %calltmp2 = call double @explode()
  %addtmp3 = fadd double %addtmp, %calltmp2
  store double %addtmp3, ptr @bigChungus, align 8
  store double 5.500000e+01, ptr @sumTest, align 8
  ret double 5.500000e+01
}

define internal void @_init_global_p() {
entry:
  store double 6.660000e+02, ptr @p, align 8
  ret void
}

define internal void @_init_global_a() {
entry:
  store double 1.000000e+01, ptr @a, align 8
  ret void
}

define internal void @_init_global_b() {
entry:
  store double 2.000000e+01, ptr @b, align 8
  ret void
}

define double @test() {
entry:
  %testONE = alloca double, align 8
  store double 1.000000e+00, ptr %testONE, align 8
  store double 0x4058FF5C28F5C28F, ptr %testONE, align 8
  %p = load double, ptr @p, align 8
  store double %p, ptr %testONE, align 8
  %p1 = load double, ptr @p, align 8
  store double %p1, ptr %testONE, align 8
  %a = load double, ptr @a, align 8
  %b = load double, ptr @b, align 8
  %addtmp = fadd double %a, %b
  %multmp = fmul double %addtmp, 3.000000e+00
  %subtmp = fsub double %multmp, 2.000000e+00
  %multmp2 = fmul double 3.000000e+00, %subtmp
  store double %multmp2, ptr %testONE, align 8
  %calltmp = call double @printd(double 1.000000e+01)
  store double %calltmp, ptr %testONE, align 8
  ret double %calltmp
}

declare double @printd(double)

define double @another(double %c) {
entry:
  %testTWO = alloca double, align 8
  %c1 = alloca double, align 8
  store double %c, ptr %c1, align 8
  store double 2.000000e+00, ptr %testTWO, align 8
  %c2 = load double, ptr %c1, align 8
  store double %c2, ptr %testTWO, align 8
  store double 1.000000e+01, ptr @p, align 8
  ret double 1.000000e+01
}

define double @explode() {
entry:
  %testTHREE = alloca double, align 8
  store double 3.000000e+00, ptr %testTHREE, align 8
  %calltmp = call double @printd(double 6.969690e+05)
  ret double %calltmp
}

define internal void @_init_global_bigChungus() {
entry:
  store double 0.000000e+00, ptr @bigChungus, align 8
  ret void
}

define internal void @_init_global_sumTest() {
entry:
  store double 0.000000e+00, ptr @sumTest, align 8
  ret void
}
