constexpr Note positive[] = {{262,188},{330,188},{392,188},{523,750},{0,0}};
constexpr Note negative[] = {{392,94},{349,94},{330,94},{294,94},{262,375},{0,0}};
constexpr Note Barbie_girl[] = {{831,240},{659,240},{831,240},{1109,240},{880,480},{0,480},{740,240},{622,240},{740,240},{988,240},{831,480},{740,240},{659,240},{0,480},{659,240},{554,240},{740,480},{554,480},{0,480},{740,240},{659,240},{831,480},{740,480},{0,0}};
constexpr Note HauntedHouse[] = {{440,1111},{659,1111},{622,1111},{494,1111},{440,1111},{523,1111},{587,1111},{466,1111},{659,1667},{659,556},{349,2222},{440,2222},{622,2222},{659,1667},{587,556},{523,1667},{494,556},{440,2222},{0,2222},{440,1111},{659,1111},{622,1111},{494,1111},{440,1111},{523,1111},{587,1111},{466,1111},{659,1667},{659,556},{349,2222},{440,2222},{622,2222},{659,1667},{587,556},{523,1667},{494,556},{440,2222},{0,0}};
constexpr Note axelf[] = {{740,375},{880,281},{740,188},{740,94},{932,188},{740,188},{659,188},{740,375},{1047,281},{740,188},{740,94},{1175,188},{1109,188},{880,188},{740,188},{1109,188},{1480,188},{740,94},{659,188},{659,94},{554,188},{831,188},{740,563},{0,0}};
constexpr Note Bond_007[] = {{523,188},{587,94},{587,94},{587,188},{587,375},{523,188},{523,188},{523,188},{523,188},{622,94},{622,94},{622,375},{587,188},{587,188},{587,188},{523,188},{587,94},{587,94},{587,188},{587,375},{523,188},{523,188},{523,188},{523,188},{622,94},{622,94},{622,188},{622,375},{587,188},{554,188},{523,188},{1047,188},{988,1125},{784,188},{699,188},{784,1125},{0,0}};
const Note *SONGS[] = {0, positive, negative, Barbie_girl, HauntedHouse, axelf, Bond_007, };
enum class RINGTONE_SONG{
    UNDEFINED,
    POSITIVE,
    NEGATIV,
    BARBIE_GIRL,
    HAUNTED_HOUSE,
    AXEL_F,
    BOND_007,
};