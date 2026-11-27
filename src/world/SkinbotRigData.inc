// Made by tools/creatures_assemble_skinbot_draft.py: each leg's joints, root to blade tip, in the mesh's own space
// (metres at scale 1; x right, y up, z forward). A section k runs from joint k to joint k+1.
inline constexpr float SkinbotJoints[4][4][3]={
 {{-0.26784f,1.16871f,0.27271f},{-0.33320f,0.53308f,0.36962f},{-0.53405f,0.67675f,0.66739f},{-0.63495f,0.00385f,0.81698f}}, // arm_L
 {{0.26790f,1.16871f,0.27271f},{0.33327f,0.53308f,0.36962f},{0.53411f,0.67675f,0.66739f},{0.63501f,0.00385f,0.81698f}}, // arm_R
 {{-0.23109f,0.66851f,-0.35640f},{-0.30110f,0.54715f,-0.47767f},{-0.41868f,0.64730f,-0.68132f},{-0.56868f,0.00208f,-0.94113f}}, // rear_L
 {{0.23115f,0.66851f,-0.35640f},{0.30117f,0.54715f,-0.47767f},{0.41874f,0.64730f,-0.68132f},{0.56875f,0.00208f,-0.94113f}}, // rear_R
};
