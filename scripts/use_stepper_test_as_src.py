Import("env")

# Use the hardware test folder as source for this dedicated upload environment.
project_dir = env.subst("$PROJECT_DIR")
env.Replace(PROJECT_SRC_DIR=project_dir + "/test/stepper_motor_test")
