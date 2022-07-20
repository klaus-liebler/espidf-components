
import { ConfigGroup, StringItem, IntegerItem, BooleanItem, EnumItem } from './interfaces_baseclasses';


export default class{
    public Build():ConfigGroup[]{
        let i1:StringItem;
        let i2:StringItem;
        return [
            new ConfigGroup("Group1",
            [
                i1=new StringItem("Test String Item1", "foo", /[a-z]{0,3}/),
                i2=new StringItem("Test String Item2", "BAR", /[A-Z]{0,3}/),
            ]),
            new ConfigGroup("Group2",
            [
                new StringItem("Test String sub1item2", "BAR", /[A-Z]{0,3}/),
                new IntegerItem("Test Integer sub1item2", 1, 0, 10, 1),
                new BooleanItem("Test Boolean", true),
                new EnumItem("Test Enum", ["Hund", "Katze", "Maus", "Ente"]),
            ])
        ];
    }
}

