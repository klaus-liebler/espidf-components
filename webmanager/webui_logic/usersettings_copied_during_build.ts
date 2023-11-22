import { ConfigGroup, StringItem, IntegerItem, BooleanItem, EnumItem } from './usersettings_base';


export default class{
    public static Build():ConfigGroup[]{
        let i1:StringItem;
        let i2:StringItem;
        return [
            new ConfigGroup("Group1",
            [
                i1=new StringItem("Test String Item1", "foo", /.*/),
                i2=new StringItem("Test String Item2", "BAR", /.*/),
            ]),
            new ConfigGroup("Group2",
            [
                new StringItem("Test String sub1item2", "BAR", /.*/),
                new IntegerItem("Test Integer sub1item2", 1, 0, 100000, 1),
                new BooleanItem("Test Boolean", true),
                new EnumItem("Test Enum", ["Hund", "Katze", "Maus", "Ente"]),
            ])
        ];
    }
}