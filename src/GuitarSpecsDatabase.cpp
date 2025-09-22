#include "GuitarSpecsDatabase.h"

GuitarSpecsDatabase& GuitarSpecsDatabase::instance()
{
    static GuitarSpecsDatabase instance;
    return instance;
}

GuitarSpecsDatabase::GuitarSpecsDatabase()
{
    initializeDatabase();
}

void GuitarSpecsDatabase::initializeDatabase()
{
    m_specs.clear();
    m_specsByManufacturer.clear();
    
    // Initialize data for all manufacturers
    addFenderModels();
    addGibsonModels();
    addPRSModels();
    addIbanezModels();
    addESPModels();
    addJacksonModels();
    addSchecterModels();
    addEpiphoneModels();
    addSquierModels();
    addMartinModels();
    addTaylorModels();
    addYamahaModels();
    addGretschModels();
    addRickenbackerModels();
    addMusicManModels();
    addSuhrModels();
    addCharvelModels();
    addKramerModels();
    addBCRichModels();
    addDeanModels();
    addWashburnModels();
    addOvationModels();
    addGodinModels();
    addSeagullModels();
    addGuildModels();
    
    // Organize by manufacturer
    for (const auto& spec : m_specs) {
        GuitarManufacturer mfg = static_cast<GuitarManufacturer>(0);
        if (spec.manufacturer == "Fender") mfg = GuitarManufacturer::Fender;
        else if (spec.manufacturer == "Gibson") mfg = GuitarManufacturer::Gibson;
        else if (spec.manufacturer == "PRS") mfg = GuitarManufacturer::PRS;
        else if (spec.manufacturer == "Ibanez") mfg = GuitarManufacturer::Ibanez;
        else if (spec.manufacturer == "ESP") mfg = GuitarManufacturer::ESP;
        else if (spec.manufacturer == "Jackson") mfg = GuitarManufacturer::Jackson;
        else if (spec.manufacturer == "Schecter") mfg = GuitarManufacturer::Schecter;
        else if (spec.manufacturer == "Epiphone") mfg = GuitarManufacturer::Epiphone;
        else if (spec.manufacturer == "Squier") mfg = GuitarManufacturer::Squier;
        else if (spec.manufacturer == "Martin") mfg = GuitarManufacturer::Martin;
        else if (spec.manufacturer == "Taylor") mfg = GuitarManufacturer::Taylor;
        else if (spec.manufacturer == "Yamaha") mfg = GuitarManufacturer::Yamaha;
        else if (spec.manufacturer == "Gretsch") mfg = GuitarManufacturer::Gretsch;
        else if (spec.manufacturer == "Rickenbacker") mfg = GuitarManufacturer::Rickenbacker;
        else if (spec.manufacturer == "Music Man") mfg = GuitarManufacturer::MusicMan;
        else if (spec.manufacturer == "Suhr") mfg = GuitarManufacturer::Suhr;
        else if (spec.manufacturer == "Charvel") mfg = GuitarManufacturer::Charvel;
        else if (spec.manufacturer == "Kramer") mfg = GuitarManufacturer::Kramer;
        else if (spec.manufacturer == "BC Rich") mfg = GuitarManufacturer::BCRich;
        else if (spec.manufacturer == "Dean") mfg = GuitarManufacturer::Dean;
        else if (spec.manufacturer == "Washburn") mfg = GuitarManufacturer::Washburn;
        else if (spec.manufacturer == "Ovation") mfg = GuitarManufacturer::Ovation;
        else if (spec.manufacturer == "Godin") mfg = GuitarManufacturer::Godin;
        else if (spec.manufacturer == "Seagull") mfg = GuitarManufacturer::Seagull;
        else if (spec.manufacturer == "Guild") mfg = GuitarManufacturer::Guild;
        
        m_specsByManufacturer[mfg].append(spec);
    }
}

void GuitarSpecsDatabase::addFenderModels()
{
    // Fender Stratocaster
    GuitarSpecs strat;
    strat.manufacturer = "Fender";
    strat.model = "Stratocaster";
    strat.series = "American Professional II";
    strat.yearIntroduced = 1954;
    strat.description = "The iconic Stratocaster - versatile, comfortable, and timeless";
    strat.bodyStyle = GuitarBodyStyle::SolidBody;
    strat.bodyLength = 457;
    strat.bodyWidth = 324;
    strat.bodyThickness = 44.5;
    strat.upperBoutWidth = 280;
    strat.lowerBoutWidth = 324;
    strat.waistWidth = 210;
    strat.bodyWood = "Alder";
    strat.bodyFinish = "Polyurethane";
    strat.scaleLength = 648;
    strat.neckLength = 648;
    strat.numberOfFrets = 22;
    strat.nutWidth = 42;
    strat.neckThickness1stFret = 20.6;
    strat.neckThickness12thFret = 22.2;
    strat.neckWood = "Maple";
    strat.fretboardWood = "Maple/Rosewood";
    strat.neckProfile = "Modern C";
    strat.neckJoint = "Bolt-on";
    strat.headstockLength = 165;
    strat.headstockWidth = 89;
    strat.headstockStyle = "6-in-line";
    strat.pickupConfig = PickupConfiguration::SSS;
    strat.pickupTypes = {"V-Mod II Single-Coil", "V-Mod II Single-Coil", "V-Mod II Single-Coil"};
    strat.bridge = "2-Point Synchronized Tremolo";
    strat.tailpiece = "Integrated";
    strat.tuners = "Fender Deluxe Cast/Sealed";
    strat.numberOfStrings = 6;
    strat.numberOfVolumes = 1;
    strat.numberOfTones = 2;
    strat.switchType = "5-Position Blade";
    strat.wiring = "Standard Stratocaster";
    strat.specialFeatures = {"Tremolo Bridge", "Contoured Body", "22 Narrow Tall Frets"};
    strat.priceRange = "$1,500 - $2,000";
    m_specs.append(strat);
    
    // Fender Telecaster
    GuitarSpecs tele;
    tele.manufacturer = "Fender";
    tele.model = "Telecaster";
    tele.series = "American Professional II";
    tele.yearIntroduced = 1950;
    tele.description = "The original solid-body electric guitar - simple, reliable, and cutting";
    tele.bodyStyle = GuitarBodyStyle::SolidBody;
    tele.bodyLength = 457;
    tele.bodyWidth = 324;
    tele.bodyThickness = 44.5;
    tele.upperBoutWidth = 280;
    tele.lowerBoutWidth = 324;
    tele.waistWidth = 210;
    tele.bodyWood = "Alder";
    tele.bodyFinish = "Polyurethane";
    tele.scaleLength = 648;
    tele.neckLength = 648;
    tele.numberOfFrets = 22;
    tele.nutWidth = 42;
    tele.neckThickness1stFret = 20.6;
    tele.neckThickness12thFret = 22.2;
    tele.neckWood = "Maple";
    tele.fretboardWood = "Maple";
    tele.neckProfile = "Modern C";
    tele.neckJoint = "Bolt-on";
    tele.headstockLength = 165;
    tele.headstockWidth = 89;
    tele.headstockStyle = "6-in-line";
    tele.pickupConfig = PickupConfiguration::SSS;
    tele.pickupTypes = {"V-Mod II Telecaster Single-Coil", "V-Mod II Telecaster Single-Coil"};
    tele.bridge = "6-Saddle String-Through-Body";
    tele.tailpiece = "Integrated";
    tele.tuners = "Fender Deluxe Cast/Sealed";
    tele.numberOfStrings = 6;
    tele.numberOfVolumes = 1;
    tele.numberOfTones = 1;
    tele.switchType = "3-Position Blade";
    tele.wiring = "Standard Telecaster";
    tele.specialFeatures = {"Single-Cutaway", "String-Through-Body", "Metal Control Plate"};
    tele.priceRange = "$1,500 - $2,000";
    m_specs.append(tele);
    
    // Fender Jazzmaster
    GuitarSpecs jazz;
    jazz.manufacturer = "Fender";
    jazz.model = "Jazzmaster";
    jazz.series = "American Professional II";
    jazz.yearIntroduced = 1958;
    jazz.description = "Offset body with unique tremolo system and distinctive tone";
    jazz.bodyStyle = GuitarBodyStyle::SolidBody;
    jazz.bodyLength = 483;
    jazz.bodyWidth = 343;
    jazz.bodyThickness = 44.5;
    jazz.upperBoutWidth = 270;
    jazz.lowerBoutWidth = 343;
    jazz.waistWidth = 200;
    jazz.bodyWood = "Alder";
    jazz.bodyFinish = "Polyurethane";
    jazz.scaleLength = 648;
    jazz.neckLength = 648;
    jazz.numberOfFrets = 22;
    jazz.nutWidth = 42;
    jazz.neckThickness1stFret = 20.6;
    jazz.neckThickness12thFret = 22.2;
    jazz.neckWood = "Maple";
    jazz.fretboardWood = "Rosewood";
    jazz.neckProfile = "Modern C";
    jazz.neckJoint = "Bolt-on";
    jazz.headstockLength = 165;
    jazz.headstockWidth = 89;
    jazz.headstockStyle = "6-in-line";
    jazz.pickupConfig = PickupConfiguration::SSS;
    jazz.pickupTypes = {"V-Mod II Jazzmaster Single-Coil", "V-Mod II Jazzmaster Single-Coil"};
    jazz.bridge = "Jazzmaster Floating Tremolo";
    jazz.tailpiece = "Integrated";
    jazz.tuners = "Fender Deluxe Cast/Sealed";
    jazz.numberOfStrings = 6;
    jazz.numberOfVolumes = 1;
    jazz.numberOfTones = 1;
    jazz.switchType = "3-Position Toggle";
    jazz.wiring = "Jazzmaster";
    jazz.specialFeatures = {"Offset Body", "Floating Tremolo", "Lead/Rhythm Circuit"};
    jazz.priceRange = "$1,700 - $2,200";
    m_specs.append(jazz);
    
    // Fender Mustang
    GuitarSpecs mustang;
    mustang.manufacturer = "Fender";
    mustang.model = "Mustang";
    mustang.series = "Player";
    mustang.yearIntroduced = 1964;
    mustang.description = "Compact scale guitar with vibrant punch and articulate tone";
    mustang.bodyStyle = GuitarBodyStyle::SolidBody;
    mustang.bodyLength = 420;
    mustang.bodyWidth = 305;
    mustang.bodyThickness = 44.5;
    mustang.upperBoutWidth = 260;
    mustang.lowerBoutWidth = 305;
    mustang.waistWidth = 190;
    mustang.bodyWood = "Alder";
    mustang.bodyFinish = "Polyester";
    mustang.scaleLength = 610;
    mustang.neckLength = 610;
    mustang.numberOfFrets = 22;
    mustang.nutWidth = 42;
    mustang.neckThickness1stFret = 20.3;
    mustang.neckThickness12thFret = 21.8;
    mustang.neckWood = "Maple";
    mustang.fretboardWood = "Pau Ferro";
    mustang.neckProfile = "Modern C";
    mustang.neckJoint = "Bolt-on";
    mustang.headstockLength = 160;
    mustang.headstockWidth = 85;
    mustang.headstockStyle = "6-in-line";
    mustang.pickupConfig = PickupConfiguration::HH;
    mustang.pickupTypes = {"Player Humbucker", "Player Humbucker"};
    mustang.bridge = "Dynamic Vibrato";
    mustang.tailpiece = "Integrated";
    mustang.tuners = "Standard Cast/Sealed";
    mustang.numberOfStrings = 6;
    mustang.numberOfVolumes = 2;
    mustang.numberOfTones = 1;
    mustang.switchType = "3-Position Toggle";
    mustang.wiring = "Standard";
    mustang.specialFeatures = {"Short Scale", "Dynamic Vibrato", "Offset Body"};
    mustang.priceRange = "$800 - $1,200";
    m_specs.append(mustang);
    
    // Fender Jaguar
    GuitarSpecs jaguar;
    jaguar.manufacturer = "Fender";
    jaguar.model = "Jaguar";
    jaguar.series = "American Professional II";
    jaguar.yearIntroduced = 1962;
    jaguar.description = "Short-scale offset with complex switching and tremolo";
    jaguar.bodyStyle = GuitarBodyStyle::SolidBody;
    jaguar.bodyLength = 432;
    jaguar.bodyWidth = 318;
    jaguar.bodyThickness = 44.5;
    jaguar.upperBoutWidth = 270;
    jaguar.lowerBoutWidth = 318;
    jaguar.waistWidth = 195;
    jaguar.bodyWood = "Alder";
    jaguar.bodyFinish = "Polyurethane";
    jaguar.scaleLength = 610;
    jaguar.neckLength = 610;
    jaguar.numberOfFrets = 22;
    jaguar.nutWidth = 42;
    jaguar.neckThickness1stFret = 20.6;
    jaguar.neckThickness12thFret = 22.2;
    jaguar.neckWood = "Maple";
    jaguar.fretboardWood = "Rosewood";
    jaguar.neckProfile = "Modern C";
    jaguar.neckJoint = "Bolt-on";
    jaguar.headstockLength = 160;
    jaguar.headstockWidth = 85;
    jaguar.headstockStyle = "6-in-line";
    jaguar.pickupConfig = PickupConfiguration::SS;
    jaguar.pickupTypes = {"V-Mod II Jaguar Single-Coil", "V-Mod II Jaguar Single-Coil"};
    jaguar.bridge = "Vintage-Style Floating Tremolo";
    jaguar.tailpiece = "Separate";
    jaguar.tuners = "Fender Deluxe Cast/Sealed";
    jaguar.numberOfStrings = 6;
    jaguar.numberOfVolumes = 2;
    jaguar.numberOfTones = 2;
    jaguar.switchType = "3-Position Toggle + Lead/Rhythm";
    jaguar.wiring = "Jaguar";
    jaguar.specialFeatures = {"Short Scale", "Complex Switching", "Floating Tremolo"};
    jaguar.priceRange = "$1,600 - $2,100";
    m_specs.append(jaguar);
    
    // Fender Precision Bass
    GuitarSpecs pbass;
    pbass.manufacturer = "Fender";
    pbass.model = "Precision Bass";
    pbass.series = "American Professional II";
    pbass.yearIntroduced = 1951;
    pbass.description = "The original electric bass - punchy, defined, and fundamental";
    pbass.bodyStyle = GuitarBodyStyle::SolidBody;
    pbass.bodyLength = 495;
    pbass.bodyWidth = 335;
    pbass.bodyThickness = 44.5;
    pbass.upperBoutWidth = 285;
    pbass.lowerBoutWidth = 335;
    pbass.waistWidth = 215;
    pbass.bodyWood = "Alder";
    pbass.bodyFinish = "Polyurethane";
    pbass.scaleLength = 864;
    pbass.neckLength = 864;
    pbass.numberOfFrets = 20;
    pbass.nutWidth = 42;
    pbass.neckThickness1stFret = 21.5;
    pbass.neckThickness12thFret = 23.5;
    pbass.neckWood = "Maple";
    pbass.fretboardWood = "Maple/Rosewood";
    pbass.neckProfile = "Modern C";
    pbass.neckJoint = "Bolt-on";
    pbass.headstockLength = 185;
    pbass.headstockWidth = 95;
    pbass.headstockStyle = "4-in-line";
    pbass.pickupConfig = PickupConfiguration::P;
    pbass.pickupTypes = {"V-Mod II Precision Bass Split-Coil"};
    pbass.bridge = "4-Saddle HiMass Vintage";
    pbass.tailpiece = "Integrated";
    pbass.tuners = "Fender HiMass Bass";
    pbass.numberOfStrings = 4;
    pbass.numberOfVolumes = 1;
    pbass.numberOfTones = 1;
    pbass.switchType = "None";
    pbass.wiring = "Standard Bass";
    pbass.specialFeatures = {"Split-Coil Pickup", "Vintage Bridge", "Long Scale"};
    pbass.priceRange = "$1,500 - $2,000";
    m_specs.append(pbass);
    
    // Fender Jazz Bass
    GuitarSpecs jbass;
    jbass.manufacturer = "Fender";
    jbass.model = "Jazz Bass";
    jbass.series = "American Professional II";
    jbass.yearIntroduced = 1960;
    jbass.description = "Versatile bass with bright tone and flexible control";
    jbass.bodyStyle = GuitarBodyStyle::SolidBody;
    jbass.bodyLength = 495;
    jbass.bodyWidth = 330;
    jbass.bodyThickness = 44.5;
    jbass.upperBoutWidth = 280;
    jbass.lowerBoutWidth = 330;
    jbass.waistWidth = 210;
    jbass.bodyWood = "Alder";
    jbass.bodyFinish = "Polyurethane";
    jbass.scaleLength = 864;
    jbass.neckLength = 864;
    jbass.numberOfFrets = 20;
    jbass.nutWidth = 38;
    jbass.neckThickness1stFret = 20.6;
    jbass.neckThickness12thFret = 22.2;
    jbass.neckWood = "Maple";
    jbass.fretboardWood = "Rosewood";
    jbass.neckProfile = "Modern C";
    jbass.neckJoint = "Bolt-on";
    jbass.headstockLength = 185;
    jbass.headstockWidth = 95;
    jbass.headstockStyle = "4-in-line";
    jbass.pickupConfig = PickupConfiguration::JJ;
    jbass.pickupTypes = {"V-Mod II Jazz Bass Single-Coil", "V-Mod II Jazz Bass Single-Coil"};
    jbass.bridge = "4-Saddle HiMass Vintage";
    jbass.tailpiece = "Integrated";
    jbass.tuners = "Fender HiMass Bass";
    jbass.numberOfStrings = 4;
    jbass.numberOfVolumes = 2;
    jbass.numberOfTones = 1;
    jbass.switchType = "None";
    jbass.wiring = "Standard Jazz Bass";
    jbass.specialFeatures = {"Dual Single-Coils", "Offset Waist", "Bright Tone"};
    jbass.priceRange = "$1,500 - $2,000";
    m_specs.append(jbass);
    
    // Fender Starcaster
    GuitarSpecs starcaster;
    starcaster.manufacturer = "Fender";
    starcaster.model = "Starcaster";
    starcaster.series = "Modern Player";
    starcaster.yearIntroduced = 1976;
    starcaster.description = "Semi-hollow offset with unique tone and feel";
    starcaster.bodyStyle = GuitarBodyStyle::SemiHollow;
    starcaster.bodyLength = 445;
    starcaster.bodyWidth = 320;
    starcaster.bodyThickness = 44.5;
    starcaster.upperBoutWidth = 275;
    starcaster.lowerBoutWidth = 320;
    starcaster.waistWidth = 200;
    starcaster.bodyWood = "Maple";
    starcaster.bodyFinish = "Polyester";
    starcaster.scaleLength = 648;
    starcaster.neckLength = 648;
    starcaster.numberOfFrets = 22;
    starcaster.nutWidth = 42;
    starcaster.neckThickness1stFret = 20.8;
    starcaster.neckThickness12thFret = 22.5;
    starcaster.neckWood = "Maple";
    starcaster.fretboardWood = "Pau Ferro";
    starcaster.neckProfile = "Modern C";
    starcaster.neckJoint = "Bolt-on";
    starcaster.headstockLength = 165;
    starcaster.headstockWidth = 89;
    starcaster.headstockStyle = "6-in-line";
    starcaster.pickupConfig = PickupConfiguration::HH;
    starcaster.pickupTypes = {"MP-90 Soapbar", "MP-90 Soapbar"};
    starcaster.bridge = "Adjusto-Matic";
    starcaster.tailpiece = "Separate";
    starcaster.tuners = "Standard Cast/Sealed";
    starcaster.numberOfStrings = 6;
    starcaster.numberOfVolumes = 2;
    starcaster.numberOfTones = 2;
    starcaster.switchType = "3-Position Toggle";
    starcaster.wiring = "Standard";
    starcaster.specialFeatures = {"Semi-Hollow", "Offset Body", "F-Hole"};
    starcaster.priceRange = "$700 - $1,000";
    m_specs.append(starcaster);
}

void GuitarSpecsDatabase::addGibsonModels()
{
    // Gibson Les Paul Standard
    GuitarSpecs lespaul;
    lespaul.manufacturer = "Gibson";
    lespaul.model = "Les Paul Standard";
    lespaul.series = "Standard 50s";
    lespaul.yearIntroduced = 1952;
    lespaul.description = "The legendary Les Paul - thick tone, sustain, and classic rock sound";
    lespaul.bodyStyle = GuitarBodyStyle::SolidBody;
    lespaul.bodyLength = 457;
    lespaul.bodyWidth = 340;
    lespaul.bodyThickness = 51;
    lespaul.upperBoutWidth = 340;
    lespaul.lowerBoutWidth = 340;
    lespaul.waistWidth = 240;
    lespaul.bodyWood = "Mahogany with Maple Cap";
    lespaul.bodyFinish = "Nitrocellulose Lacquer";
    lespaul.scaleLength = 628;
    lespaul.neckLength = 628;
    lespaul.numberOfFrets = 22;
    lespaul.nutWidth = 43;
    lespaul.neckThickness1stFret = 21.4;
    lespaul.neckThickness12thFret = 23.8;
    lespaul.neckWood = "Mahogany";
    lespaul.fretboardWood = "Rosewood";
    lespaul.neckProfile = "50s Rounded";
    lespaul.neckJoint = "Set Neck";
    lespaul.headstockLength = 152;
    lespaul.headstockWidth = 89;
    lespaul.headstockStyle = "3+3";
    lespaul.pickupConfig = PickupConfiguration::HH;
    lespaul.pickupTypes = {"Burstbucker Pro Humbucker", "Burstbucker Pro Humbucker"};
    lespaul.bridge = "Tune-O-Matic";
    lespaul.tailpiece = "Stopbar";
    lespaul.tuners = "Grover Rotomatic";
    lespaul.numberOfStrings = 6;
    lespaul.numberOfVolumes = 2;
    lespaul.numberOfTones = 2;
    lespaul.switchType = "3-Position Toggle";
    lespaul.wiring = "50s Wiring";
    lespaul.specialFeatures = {"Carved Maple Cap", "Binding", "Block Inlays"};
    lespaul.priceRange = "$2,500 - $3,500";
    m_specs.append(lespaul);
    
    // Gibson SG Standard
    GuitarSpecs sg;
    sg.manufacturer = "Gibson";
    sg.model = "SG Standard";
    sg.series = "Standard";
    sg.yearIntroduced = 1961;
    sg.description = "Devil horn design with easy upper fret access and aggressive tone";
    sg.bodyStyle = GuitarBodyStyle::SolidBody;
    sg.bodyLength = 406;
    sg.bodyWidth = 318;
    sg.bodyThickness = 32;
    sg.upperBoutWidth = 250;
    sg.lowerBoutWidth = 250;
    sg.waistWidth = 180;
    sg.bodyWood = "Mahogany";
    sg.bodyFinish = "Nitrocellulose Lacquer";
    sg.scaleLength = 628;
    sg.neckLength = 628;
    sg.numberOfFrets = 22;
    sg.nutWidth = 43;
    sg.neckThickness1stFret = 20.3;
    sg.neckThickness12thFret = 22.5;
    sg.neckWood = "Mahogany";
    sg.fretboardWood = "Rosewood";
    sg.neckProfile = "Slim Taper";
    sg.neckJoint = "Set Neck";
    sg.headstockLength = 152;
    sg.headstockWidth = 89;
    sg.headstockStyle = "3+3";
    sg.pickupConfig = PickupConfiguration::HH;
    sg.pickupTypes = {"490R Humbucker", "498T Humbucker"};
    sg.bridge = "Tune-O-Matic";
    sg.tailpiece = "Stopbar";
    sg.tuners = "Grover Rotomatic";
    sg.numberOfStrings = 6;
    sg.numberOfVolumes = 2;
    sg.numberOfTones = 2;
    sg.switchType = "3-Position Toggle";
    sg.wiring = "Modern Wiring";
    sg.specialFeatures = {"Double Cutaway", "Devil Horns", "Easy Upper Fret Access"};
    sg.priceRange = "$1,800 - $2,500";
    m_specs.append(sg);
    
    // Gibson Flying V
    GuitarSpecs flyingv;
    flyingv.manufacturer = "Gibson";
    flyingv.model = "Flying V";
    flyingv.series = "Standard";
    flyingv.yearIntroduced = 1958;
    flyingv.description = "Radical V-shaped design with aggressive looks and tone";
    flyingv.bodyStyle = GuitarBodyStyle::SolidBody;
    flyingv.bodyLength = 406;
    flyingv.bodyWidth = 380; // Variable due to V-shape
    flyingv.bodyThickness = 44;
    flyingv.upperBoutWidth = 200;
    flyingv.lowerBoutWidth = 380;
    flyingv.waistWidth = 150;
    flyingv.bodyWood = "Mahogany";
    flyingv.bodyFinish = "Nitrocellulose Lacquer";
    flyingv.scaleLength = 628;
    flyingv.neckLength = 628;
    flyingv.numberOfFrets = 22;
    flyingv.nutWidth = 43;
    flyingv.neckThickness1stFret = 20.3;
    flyingv.neckThickness12thFret = 22.5;
    flyingv.neckWood = "Mahogany";
    flyingv.fretboardWood = "Rosewood";
    flyingv.neckProfile = "Slim Taper";
    flyingv.neckJoint = "Set Neck";
    flyingv.headstockLength = 152;
    flyingv.headstockWidth = 89;
    flyingv.headstockStyle = "3+3";
    flyingv.pickupConfig = PickupConfiguration::HH;
    flyingv.pickupTypes = {"490R Humbucker", "498T Humbucker"};
    flyingv.bridge = "Tune-O-Matic";
    flyingv.tailpiece = "Stopbar";
    flyingv.tuners = "Grover Rotomatic";
    flyingv.numberOfStrings = 6;
    flyingv.numberOfVolumes = 2;
    flyingv.numberOfTones = 1;
    flyingv.switchType = "3-Position Toggle";
    flyingv.wiring = "Modern Wiring";
    flyingv.specialFeatures = {"V-Shape Body", "Radical Design", "Iconic"};
    flyingv.priceRange = "$1,800 - $2,500";
    m_specs.append(flyingv);
    
    // Gibson ES-335
    GuitarSpecs es335;
    es335.manufacturer = "Gibson";
    es335.model = "ES-335";
    es335.series = "Dot";
    es335.yearIntroduced = 1958;
    es335.description = "Semi-hollow masterpiece with warm, rich tone";
    es335.bodyStyle = GuitarBodyStyle::SemiHollow;
    es335.bodyLength = 457;
    es335.bodyWidth = 406;
    es335.bodyThickness = 44;
    es335.upperBoutWidth = 280;
    es335.lowerBoutWidth = 406;
    es335.waistWidth = 270;
    es335.bodyWood = "Maple/Poplar";
    es335.bodyFinish = "Nitrocellulose Lacquer";
    es335.scaleLength = 628;
    es335.neckLength = 628;
    es335.numberOfFrets = 22;
    es335.nutWidth = 43;
    es335.neckThickness1stFret = 20.8;
    es335.neckThickness12thFret = 22.8;
    es335.neckWood = "Mahogany";
    es335.fretboardWood = "Rosewood";
    es335.neckProfile = "Rounded C";
    es335.neckJoint = "Set Neck";
    es335.headstockLength = 152;
    es335.headstockWidth = 89;
    es335.headstockStyle = "3+3";
    es335.pickupConfig = PickupConfiguration::HH;
    es335.pickupTypes = {"490R Humbucker", "490T Humbucker"};
    es335.bridge = "ABR-1 Tune-O-Matic";
    es335.tailpiece = "Stopbar";
    es335.tuners = "Grover Rotomatic";
    es335.numberOfStrings = 6;
    es335.numberOfVolumes = 2;
    es335.numberOfTones = 2;
    es335.switchType = "3-Position Toggle";
    es335.wiring = "Modern Wiring";
    es335.specialFeatures = {"Semi-Hollow", "F-Holes", "Center Block"};
    es335.priceRange = "$2,500 - $3,500";
    m_specs.append(es335);
    
    // Gibson Explorer
    GuitarSpecs explorer;
    explorer.manufacturer = "Gibson";
    explorer.model = "Explorer";
    explorer.series = "Standard";
    explorer.yearIntroduced = 1958;
    explorer.description = "Modernistic design with aggressive tone";
    explorer.bodyStyle = GuitarBodyStyle::SolidBody;
    explorer.bodyLength = 430;
    explorer.bodyWidth = 350;
    explorer.bodyThickness = 44;
    explorer.upperBoutWidth = 250;
    explorer.lowerBoutWidth = 350;
    explorer.waistWidth = 180;
    explorer.bodyWood = "Mahogany";
    explorer.bodyFinish = "Nitrocellulose Lacquer";
    explorer.scaleLength = 628;
    explorer.neckLength = 628;
    explorer.numberOfFrets = 22;
    explorer.nutWidth = 43;
    explorer.neckThickness1stFret = 20.3;
    explorer.neckThickness12thFret = 22.5;
    explorer.neckWood = "Mahogany";
    explorer.fretboardWood = "Rosewood";
    explorer.neckProfile = "Slim Taper";
    explorer.neckJoint = "Set Neck";
    explorer.headstockLength = 152;
    explorer.headstockWidth = 89;
    explorer.headstockStyle = "3+3";
    explorer.pickupConfig = PickupConfiguration::HH;
    explorer.pickupTypes = {"490R Humbucker", "498T Humbucker"};
    explorer.bridge = "Tune-O-Matic";
    explorer.tailpiece = "Stopbar";
    explorer.tuners = "Grover Rotomatic";
    explorer.numberOfStrings = 6;
    explorer.numberOfVolumes = 2;
    explorer.numberOfTones = 1;
    explorer.switchType = "3-Position Toggle";
    explorer.wiring = "Modern Wiring";
    explorer.specialFeatures = {"Korina Wood Option", "Radical Design", "Sustain"};
    explorer.priceRange = "$1,700 - $2,400";
    m_specs.append(explorer);
    
    // Gibson ES-175
    GuitarSpecs es175;
    es175.manufacturer = "Gibson";
    es175.model = "ES-175";
    es175.series = "Standard";
    es175.yearIntroduced = 1949;
    es175.description = "Classic archtop jazz guitar with elegant tone";
    es175.bodyStyle = GuitarBodyStyle::HollowBody;
    es175.bodyLength = 457;
    es175.bodyWidth = 406;
    es175.bodyThickness = 83;
    es175.upperBoutWidth = 280;
    es175.lowerBoutWidth = 406;
    es175.waistWidth = 270;
    es175.bodyWood = "Maple";
    es175.bodyFinish = "Nitrocellulose Lacquer";
    es175.scaleLength = 628;
    es175.neckLength = 628;
    es175.numberOfFrets = 20;
    es175.nutWidth = 44;
    es175.neckThickness1stFret = 21.5;
    es175.neckThickness12thFret = 23.8;
    es175.neckWood = "Mahogany";
    es175.fretboardWood = "Ebony";
    es175.neckProfile = "Rounded C";
    es175.neckJoint = "Set Neck";
    es175.headstockLength = 152;
    es175.headstockWidth = 89;
    es175.headstockStyle = "3+3";
    es175.pickupConfig = PickupConfiguration::HH;
    es175.pickupTypes = {"490R Humbucker", "490T Humbucker"};
    es175.bridge = "Ebony Floating";
    es175.tailpiece = "Trapeze";
    es175.tuners = "Grover Rotomatic";
    es175.numberOfStrings = 6;
    es175.numberOfVolumes = 2;
    es175.numberOfTones = 2;
    es175.switchType = "3-Position Toggle";
    es175.wiring = "Traditional";
    es175.specialFeatures = {"Archtop", "Floating Bridge", "Jazz Tone"};
    es175.priceRange = "$3,000 - $4,500";
    m_specs.append(es175);
    
    // Gibson Firebird
    GuitarSpecs firebird;
    firebird.manufacturer = "Gibson";
    firebird.model = "Firebird";
    firebird.series = "Studio";
    firebird.yearIntroduced = 1963;
    firebird.description = "Reverse-body design with neck-through construction";
    firebird.bodyStyle = GuitarBodyStyle::SolidBody;
    firebird.bodyLength = 445;
    firebird.bodyWidth = 330;
    firebird.bodyThickness = 38;
    firebird.upperBoutWidth = 250;
    firebird.lowerBoutWidth = 330;
    firebird.waistWidth = 190;
    firebird.bodyWood = "Mahogany";
    firebird.bodyFinish = "Nitrocellulose Lacquer";
    firebird.scaleLength = 628;
    firebird.neckLength = 628;
    firebird.numberOfFrets = 22;
    firebird.nutWidth = 43;
    firebird.neckThickness1stFret = 20.1;
    firebird.neckThickness12thFret = 21.8;
    firebird.neckWood = "Mahogany";
    firebird.fretboardWood = "Rosewood";
    firebird.neckProfile = "Slim Taper";
    firebird.neckJoint = "Neck-Through";
    firebird.headstockLength = 152;
    firebird.headstockWidth = 89;
    firebird.headstockStyle = "6-in-line";
    firebird.pickupConfig = PickupConfiguration::HH;
    firebird.pickupTypes = {"490R Humbucker", "490T Humbucker"};
    firebird.bridge = "Tune-O-Matic";
    firebird.tailpiece = "Maestro Vibrola";
    firebird.tuners = "Mini Grover";
    firebird.numberOfStrings = 6;
    firebird.numberOfVolumes = 2;
    firebird.numberOfTones = 1;
    firebird.switchType = "3-Position Toggle";
    firebird.wiring = "Modern Wiring";
    firebird.specialFeatures = {"Reverse Body", "Neck-Through", "Banjo Tuners"};
    firebird.priceRange = "$1,400 - $2,000";
    m_specs.append(firebird);
    
    // Gibson Thunderbird Bass
    GuitarSpecs tbird;
    tbird.manufacturer = "Gibson";
    tbird.model = "Thunderbird";
    tbird.series = "Studio";
    tbird.yearIntroduced = 1963;
    tbird.description = "Iconic bass with powerful tone and neck-through design";
    tbird.bodyStyle = GuitarBodyStyle::SolidBody;
    tbird.bodyLength = 480;
    tbird.bodyWidth = 350;
    tbird.bodyThickness = 38;
    tbird.upperBoutWidth = 250;
    tbird.lowerBoutWidth = 350;
    tbird.waistWidth = 190;
    tbird.bodyWood = "Mahogany";
    tbird.bodyFinish = "Nitrocellulose Lacquer";
    tbird.scaleLength = 864;
    tbird.neckLength = 864;
    tbird.numberOfFrets = 20;
    tbird.nutWidth = 40;
    tbird.neckThickness1stFret = 21.0;
    tbird.neckThickness12thFret = 22.5;
    tbird.neckWood = "Mahogany";
    tbird.fretboardWood = "Rosewood";
    tbird.neckProfile = "Slim Taper";
    tbird.neckJoint = "Neck-Through";
    tbird.headstockLength = 185;
    tbird.headstockWidth = 95;
    tbird.headstockStyle = "4-in-line";
    tbird.pickupConfig = PickupConfiguration::HH;
    tbird.pickupTypes = {"TB+ Humbucker", "TB+ Humbucker"};
    tbird.bridge = "3-Point Adjustable";
    tbird.tailpiece = "Integrated";
    tbird.tuners = "Mini Grover";
    tbird.numberOfStrings = 4;
    tbird.numberOfVolumes = 2;
    tbird.numberOfTones = 1;
    tbird.switchType = "3-Position Toggle";
    tbird.wiring = "Standard Bass";
    tbird.specialFeatures = {"Reverse Body", "Neck-Through", "Powerful Tone"};
    tbird.priceRange = "$1,200 - $1,800";
    m_specs.append(tbird);
}

void GuitarSpecsDatabase::addPRSModels()
{
    // PRS Custom 24
    GuitarSpecs custom24;
    custom24.manufacturer = "PRS";
    custom24.model = "Custom 24";
    custom24.series = "Core";
    custom24.yearIntroduced = 1985;
    custom24.description = "The flagship PRS model combining Gibson and Fender characteristics";
    custom24.bodyStyle = GuitarBodyStyle::SolidBody;
    custom24.bodyLength = 457;
    custom24.bodyWidth = 330;
    custom24.bodyThickness = 51;
    custom24.upperBoutWidth = 300;
    custom24.lowerBoutWidth = 330;
    custom24.waistWidth = 220;
    custom24.bodyWood = "Mahogany with Maple Cap";
    custom24.bodyFinish = "Nitrocellulose Lacquer";
    custom24.scaleLength = 635;
    custom24.neckLength = 635;
    custom24.numberOfFrets = 24;
    custom24.nutWidth = 43;
    custom24.neckThickness1stFret = 21;
    custom24.neckThickness12thFret = 23;
    custom24.neckWood = "Mahogany";
    custom24.fretboardWood = "Rosewood";
    custom24.neckProfile = "Pattern Regular";
    custom24.neckJoint = "Set Neck";
    custom24.headstockLength = 140;
    custom24.headstockWidth = 89;
    custom24.headstockStyle = "3+3";
    custom24.pickupConfig = PickupConfiguration::HH;
    custom24.pickupTypes = {"85/15 Humbucker", "85/15 Humbucker"};
    custom24.bridge = "PRS Tremolo";
    custom24.tailpiece = "Integrated";
    custom24.tuners = "PRS Phase III Locking";
    custom24.numberOfStrings = 6;
    custom24.numberOfVolumes = 1;
    custom24.numberOfTones = 1;
    custom24.switchType = "5-Position Blade";
    custom24.wiring = "Modern with Coil Tap";
    custom24.specialFeatures = {"Bird Inlays", "Tremolo", "Coil Tap", "24 Frets"};
    custom24.priceRange = "$4,000 - $6,000";
    m_specs.append(custom24);
}

void GuitarSpecsDatabase::addIbanezModels()
{
    // Ibanez RG550
    GuitarSpecs rg550;
    rg550.manufacturer = "Ibanez";
    rg550.model = "RG550";
    rg550.series = "RG";
    rg550.yearIntroduced = 1987;
    rg550.description = "Fast neck and versatile tone for rock and metal";
    rg550.bodyStyle = GuitarBodyStyle::SolidBody;
    rg550.bodyLength = 457;
    rg550.bodyWidth = 320;
    rg550.bodyThickness = 43;
    rg550.upperBoutWidth = 280;
    rg550.lowerBoutWidth = 320;
    rg550.waistWidth = 190;
    rg550.bodyWood = "Basswood";
    rg550.bodyFinish = "Polyurethane";
    rg550.scaleLength = 648;
    rg550.neckLength = 648;
    rg550.numberOfFrets = 24;
    rg550.nutWidth = 43;
    rg550.neckThickness1stFret = 19;
    rg550.neckThickness12thFret = 21;
    rg550.neckWood = "Maple";
    rg550.fretboardWood = "Maple";
    rg550.neckProfile = "Super Wizard";
    rg550.neckJoint = "Bolt-on";
    rg550.headstockLength = 150;
    rg550.headstockWidth = 85;
    rg550.headstockStyle = "6-in-line";
    rg550.pickupConfig = PickupConfiguration::HSH;
    rg550.pickupTypes = {"V7 Humbucker", "S1 Single-Coil", "V8 Humbucker"};
    rg550.bridge = "Edge Tremolo";
    rg550.tailpiece = "Integrated";
    rg550.tuners = "Gotoh Machine Heads";
    rg550.numberOfStrings = 6;
    rg550.numberOfVolumes = 1;
    rg550.numberOfTones = 1;
    rg550.switchType = "5-Position Lever";
    rg550.wiring = "Modern HSH";
    rg550.specialFeatures = {"Double Locking Tremolo", "24 Frets", "Super Thin Neck"};
    rg550.priceRange = "$800 - $1,200";
    m_specs.append(rg550);
}

void GuitarSpecsDatabase::addESPModels()
{
    // ESP Eclipse
    GuitarSpecs eclipse;
    eclipse.manufacturer = "ESP";
    eclipse.model = "Eclipse";
    eclipse.series = "Standard";
    eclipse.yearIntroduced = 1996;
    eclipse.description = "Single cutaway with modern appointments and high-output pickups";
    eclipse.bodyStyle = GuitarBodyStyle::SolidBody;
    eclipse.bodyLength = 457;
    eclipse.bodyWidth = 340;
    eclipse.bodyThickness = 51;
    eclipse.upperBoutWidth = 340;
    eclipse.lowerBoutWidth = 340;
    eclipse.waistWidth = 240;
    eclipse.bodyWood = "Mahogany";
    eclipse.bodyFinish = "Polyurethane";
    eclipse.scaleLength = 628;
    eclipse.neckLength = 628;
    eclipse.numberOfFrets = 24;
    eclipse.nutWidth = 42;
    eclipse.neckThickness1stFret = 20;
    eclipse.neckThickness12thFret = 22;
    eclipse.neckWood = "Maple";
    eclipse.fretboardWood = "Ebony";
    eclipse.neckProfile = "Thin U";
    eclipse.neckJoint = "Set Neck";
    eclipse.headstockLength = 150;
    eclipse.headstockWidth = 85;
    eclipse.headstockStyle = "3+3";
    eclipse.pickupConfig = PickupConfiguration::HH;
    eclipse.pickupTypes = {"EMG 81 Humbucker", "EMG 60 Humbucker"};
    eclipse.bridge = "Tune-O-Matic";
    eclipse.tailpiece = "Stopbar";
    eclipse.tuners = "Gotoh";
    eclipse.numberOfStrings = 6;
    eclipse.numberOfVolumes = 2;
    eclipse.numberOfTones = 2;
    eclipse.switchType = "3-Position Toggle";
    eclipse.wiring = "Modern";
    eclipse.specialFeatures = {"24 Frets", "EMG Pickups", "Binding"};
    eclipse.priceRange = "$1,500 - $2,500";
    m_specs.append(eclipse);
}

void GuitarSpecsDatabase::addJacksonModels()
{
    // Jackson Soloist
    GuitarSpecs soloist;
    soloist.manufacturer = "Jackson";
    soloist.model = "Soloist";
    soloist.series = "Pro";
    soloist.yearIntroduced = 1984;
    soloist.description = "Neck-through construction for sustain and upper fret access";
    soloist.bodyStyle = GuitarBodyStyle::SolidBody;
    soloist.bodyLength = 457;
    soloist.bodyWidth = 320;
    soloist.bodyThickness = 43;
    soloist.upperBoutWidth = 280;
    soloist.lowerBoutWidth = 320;
    soloist.waistWidth = 190;
    soloist.bodyWood = "Poplar";
    soloist.bodyFinish = "Polyurethane";
    soloist.scaleLength = 648;
    soloist.neckLength = 648;
    soloist.numberOfFrets = 24;
    soloist.nutWidth = 43;
    soloist.neckThickness1stFret = 19;
    soloist.neckThickness12thFret = 21;
    soloist.neckWood = "Maple";
    soloist.fretboardWood = "Ebony";
    soloist.neckProfile = "Speed Neck";
    soloist.neckJoint = "Neck-Through";
    soloist.headstockLength = 140;
    soloist.headstockWidth = 85;
    soloist.headstockStyle = "6-in-line";
    soloist.pickupConfig = PickupConfiguration::HH;
    soloist.pickupTypes = {"Seymour Duncan JB Humbucker", "Seymour Duncan '59 Humbucker"};
    soloist.bridge = "Floyd Rose Special";
    soloist.tailpiece = "Integrated";
    soloist.tuners = "Jackson Sealed Die-Cast";
    soloist.numberOfStrings = 6;
    soloist.numberOfVolumes = 1;
    soloist.numberOfTones = 1;
    soloist.switchType = "3-Position Toggle";
    soloist.wiring = "Modern";
    soloist.specialFeatures = {"Neck-Through", "Floyd Rose", "24 Frets", "Shark Fin Inlays"};
    soloist.priceRange = "$1,000 - $1,800";
    m_specs.append(soloist);
}

void GuitarSpecsDatabase::addSchecterModels()
{
    // Schecter Hellraiser
    GuitarSpecs hellraiser;
    hellraiser.manufacturer = "Schecter";
    hellraiser.model = "Hellraiser";
    hellraiser.series = "C-1";
    hellraiser.yearIntroduced = 2006;
    hellraiser.description = "Modern metal machine with active pickups and aggressive styling";
    hellraiser.bodyStyle = GuitarBodyStyle::SolidBody;
    hellraiser.bodyLength = 457;
    hellraiser.bodyWidth = 340;
    hellraiser.bodyThickness = 44;
    hellraiser.upperBoutWidth = 300;
    hellraiser.lowerBoutWidth = 340;
    hellraiser.waistWidth = 220;
    hellraiser.bodyWood = "Mahogany";
    hellraiser.bodyFinish = "Satin";
    hellraiser.scaleLength = 648;
    hellraiser.neckLength = 648;
    hellraiser.numberOfFrets = 24;
    hellraiser.nutWidth = 42;
    hellraiser.neckThickness1stFret = 20;
    hellraiser.neckThickness12thFret = 22;
    hellraiser.neckWood = "Maple";
    hellraiser.fretboardWood = "Rosewood";
    hellraiser.neckProfile = "Thin C";
    hellraiser.neckJoint = "Set Neck";
    hellraiser.headstockLength = 145;
    hellraiser.headstockWidth = 85;
    hellraiser.headstockStyle = "3+3";
    hellraiser.pickupConfig = PickupConfiguration::HH;
    hellraiser.pickupTypes = {"EMG 81TW Humbucker", "EMG 89 Humbucker"};
    hellraiser.bridge = "Tune-O-Matic";
    hellraiser.tailpiece = "String-Through";
    hellraiser.tuners = "Grover";
    hellraiser.numberOfStrings = 6;
    hellraiser.numberOfVolumes = 1;
    hellraiser.numberOfTones = 1;
    hellraiser.switchType = "3-Position Toggle";
    hellraiser.wiring = "Active EMG";
    hellraiser.specialFeatures = {"Active Pickups", "Coil Tap", "Abalone Binding", "Gothic Cross Inlays"};
    hellraiser.priceRange = "$800 - $1,200";
    m_specs.append(hellraiser);
}

void GuitarSpecsDatabase::addEpiphoneModels()
{
    // Epiphone Les Paul Standard
    GuitarSpecs epiLP;
    epiLP.manufacturer = "Epiphone";
    epiLP.model = "Les Paul Standard";
    epiLP.series = "Inspired by Gibson";
    epiLP.yearIntroduced = 1989;
    epiLP.description = "Affordable Les Paul with classic design and tone";
    epiLP.bodyStyle = GuitarBodyStyle::SolidBody;
    epiLP.bodyLength = 457;
    epiLP.bodyWidth = 340;
    epiLP.bodyThickness = 51;
    epiLP.upperBoutWidth = 340;
    epiLP.lowerBoutWidth = 340;
    epiLP.waistWidth = 240;
    epiLP.bodyWood = "Mahogany";
    epiLP.bodyFinish = "Gloss";
    epiLP.scaleLength = 628;
    epiLP.neckLength = 628;
    epiLP.numberOfFrets = 22;
    epiLP.nutWidth = 43;
    epiLP.neckThickness1stFret = 21;
    epiLP.neckThickness12thFret = 23;
    epiLP.neckWood = "Mahogany";
    epiLP.fretboardWood = "Indian Laurel";
    epiLP.neckProfile = "50s Rounded";
    epiLP.neckJoint = "Set Neck";
    epiLP.headstockLength = 152;
    epiLP.headstockWidth = 89;
    epiLP.headstockStyle = "3+3";
    epiLP.pickupConfig = PickupConfiguration::HH;
    epiLP.pickupTypes = {"Alnico Classic Pro Humbucker", "Alnico Classic Pro Humbucker"};
    epiLP.bridge = "LockTone Tune-O-Matic";
    epiLP.tailpiece = "Stopbar";
    epiLP.tuners = "Epiphone Deluxe";
    epiLP.numberOfStrings = 6;
    epiLP.numberOfVolumes = 2;
    epiLP.numberOfTones = 2;
    epiLP.switchType = "3-Position Toggle";
    epiLP.wiring = "50s Wiring";
    epiLP.specialFeatures = {"Flame Maple Veneer", "Binding", "Block Inlays"};
    epiLP.priceRange = "$400 - $600";
    m_specs.append(epiLP);
}

void GuitarSpecsDatabase::addSquierModels()
{
    // Squier Classic Vibe Stratocaster
    GuitarSpecs squierStrat;
    squierStrat.manufacturer = "Squier";
    squierStrat.model = "Classic Vibe Stratocaster";
    squierStrat.series = "Classic Vibe";
    squierStrat.yearIntroduced = 2008;
    squierStrat.description = "Vintage-inspired Stratocaster with classic appointments";
    squierStrat.bodyStyle = GuitarBodyStyle::SolidBody;
    squierStrat.bodyLength = 457;
    squierStrat.bodyWidth = 324;
    squierStrat.bodyThickness = 44.5;
    squierStrat.upperBoutWidth = 280;
    squierStrat.lowerBoutWidth = 324;
    squierStrat.waistWidth = 210;
    squierStrat.bodyWood = "Poplar";
    squierStrat.bodyFinish = "Polyurethane";
    squierStrat.scaleLength = 648;
    squierStrat.neckLength = 648;
    squierStrat.numberOfFrets = 21;
    squierStrat.nutWidth = 42;
    squierStrat.neckThickness1stFret = 21;
    squierStrat.neckThickness12thFret = 23;
    squierStrat.neckWood = "Maple";
    squierStrat.fretboardWood = "Indian Laurel";
    squierStrat.neckProfile = "C Shape";
    squierStrat.neckJoint = "Bolt-on";
    squierStrat.headstockLength = 165;
    squierStrat.headstockWidth = 89;
    squierStrat.headstockStyle = "6-in-line";
    squierStrat.pickupConfig = PickupConfiguration::SSS;
    squierStrat.pickupTypes = {"Fender Designed Alnico Single-Coil", "Fender Designed Alnico Single-Coil", "Fender Designed Alnico Single-Coil"};
    squierStrat.bridge = "6-Point Synchronized Tremolo";
    squierStrat.tailpiece = "Integrated";
    squierStrat.tuners = "Vintage-Style";
    squierStrat.numberOfStrings = 6;
    squierStrat.numberOfVolumes = 1;
    squierStrat.numberOfTones = 2;
    squierStrat.switchType = "5-Position Blade";
    squierStrat.wiring = "Standard Stratocaster";
    squierStrat.specialFeatures = {"Vintage Tremolo", "Tinted Neck", "Vintage-Style Tuners"};
    squierStrat.priceRange = "$300 - $400";
    m_specs.append(squierStrat);
}

void GuitarSpecsDatabase::addMartinModels()
{
    // Martin D-28
    GuitarSpecs d28;
    d28.manufacturer = "Martin";
    d28.model = "D-28";
    d28.series = "Standard";
    d28.yearIntroduced = 1931;
    d28.description = "The quintessential dreadnought acoustic guitar";
    d28.bodyStyle = GuitarBodyStyle::Hollow;
    d28.bodyLength = 508;
    d28.bodyWidth = 394;
    d28.bodyThickness = 121;
    d28.upperBoutWidth = 279;
    d28.lowerBoutWidth = 394;
    d28.waistWidth = 279;
    d28.bodyWood = "East Indian Rosewood Back/Sides, Sitka Spruce Top";
    d28.bodyFinish = "Gloss";
    d28.scaleLength = 645;
    d28.neckLength = 645;
    d28.numberOfFrets = 20;
    d28.nutWidth = 44.5;
    d28.neckThickness1stFret = 21;
    d28.neckThickness12thFret = 25;
    d28.neckWood = "Select Hardwood";
    d28.fretboardWood = "East Indian Rosewood";
    d28.neckProfile = "Modified Low Oval";
    d28.neckJoint = "Dovetail";
    d28.headstockLength = 165;
    d28.headstockWidth = 89;
    d28.headstockStyle = "3+3";
    d28.pickupConfig = PickupConfiguration::Single;
    d28.pickupTypes = {"Acoustic (No Pickups)"};
    d28.bridge = "East Indian Rosewood";
    d28.tailpiece = "None";
    d28.tuners = "Nickel Open-Gear";
    d28.numberOfStrings = 6;
    d28.numberOfVolumes = 0;
    d28.numberOfTones = 0;
    d28.switchType = "None";
    d28.wiring = "Acoustic";
    d28.specialFeatures = {"Scalloped X-Bracing", "Bone Nut/Saddle", "Herringbone Purfling"};
    d28.priceRange = "$3,000 - $4,000";
    m_specs.append(d28);
}

void GuitarSpecsDatabase::addTaylorModels()
{
    // Taylor 814ce
    GuitarSpecs taylor814;
    taylor814.manufacturer = "Taylor";
    taylor814.model = "814ce";
    taylor814.series = "800";
    taylor814.yearIntroduced = 1994;
    taylor814.description = "Grand Auditorium with cedar top and electronics";
    taylor814.bodyStyle = GuitarBodyStyle::Hollow;
    taylor814.bodyLength = 508;
    taylor814.bodyWidth = 394;
    taylor814.bodyThickness = 114;
    taylor814.upperBoutWidth = 279;
    taylor814.lowerBoutWidth = 394;
    taylor814.waistWidth = 279;
    taylor814.bodyWood = "Indian Rosewood Back/Sides, Western Red Cedar Top";
    taylor814.bodyFinish = "Gloss";
    taylor814.scaleLength = 648;
    taylor814.neckLength = 648;
    taylor814.numberOfFrets = 20;
    taylor814.nutWidth = 44.4;
    taylor814.neckThickness1stFret = 21;
    taylor814.neckThickness12thFret = 24;
    taylor814.neckWood = "Tropical Mahogany";
    taylor814.fretboardWood = "West African Ebony";
    taylor814.neckProfile = "Taylor Performance";
    taylor814.neckJoint = "Bolt-on";
    taylor814.headstockLength = 165;
    taylor814.headstockWidth = 89;
    taylor814.headstockStyle = "3+3";
    taylor814.pickupConfig = PickupConfiguration::Single;
    taylor814.pickupTypes = {"ES2 Electronics"};
    taylor814.bridge = "West African Ebony";
    taylor814.tailpiece = "None";
    taylor814.tuners = "Taylor Nickel";
    taylor814.numberOfStrings = 6;
    taylor814.numberOfVolumes = 1;
    taylor814.numberOfTones = 1;
    taylor814.switchType = "None";
    taylor814.wiring = "ES2 Preamp";
    taylor814.specialFeatures = {"Venetian Cutaway", "ES2 Electronics", "Expression System"};
    taylor814.priceRange = "$3,500 - $4,500";
    m_specs.append(taylor814);
}

void GuitarSpecsDatabase::addYamahaModels()
{
    // Yamaha Pacifica 112V
    GuitarSpecs pacifica;
    pacifica.manufacturer = "Yamaha";
    pacifica.model = "Pacifica 112V";
    pacifica.series = "Pacifica";
    pacifica.yearIntroduced = 1990;
    pacifica.description = "Versatile guitar combining the best of Fender and Gibson designs";
    pacifica.bodyStyle = GuitarBodyStyle::SolidBody;
    pacifica.bodyLength = 457;
    pacifica.bodyWidth = 324;
    pacifica.bodyThickness = 44;
    pacifica.upperBoutWidth = 280;
    pacifica.lowerBoutWidth = 324;
    pacifica.waistWidth = 210;
    pacifica.bodyWood = "Alder";
    pacifica.bodyFinish = "Polyurethane";
    pacifica.scaleLength = 648;
    pacifica.neckLength = 648;
    pacifica.numberOfFrets = 22;
    pacifica.nutWidth = 42;
    pacifica.neckThickness1stFret = 20;
    pacifica.neckThickness12thFret = 22;
    pacifica.neckWood = "Maple";
    pacifica.fretboardWood = "Rosewood";
    pacifica.neckProfile = "C";
    pacifica.neckJoint = "Bolt-on";
    pacifica.headstockLength = 165;
    pacifica.headstockWidth = 89;
    pacifica.headstockStyle = "6-in-line";
    pacifica.pickupConfig = PickupConfiguration::HSS;
    pacifica.pickupTypes = {"Alnico V Humbucker", "Vintage Single-Coil", "Vintage Single-Coil"};
    pacifica.bridge = "Vintage-Style Tremolo";
    pacifica.tailpiece = "Integrated";
    pacifica.tuners = "Die-Cast Chrome";
    pacifica.numberOfStrings = 6;
    pacifica.numberOfVolumes = 1;
    pacifica.numberOfTones = 2;
    pacifica.switchType = "5-Position Blade";
    pacifica.wiring = "Modern HSS";
    pacifica.specialFeatures = {"Coil Tap", "Tremolo", "Block Saddles"};
    pacifica.priceRange = "$300 - $500";
    m_specs.append(pacifica);
}

void GuitarSpecsDatabase::addGretschModels()
{
    // Gretsch White Falcon
    GuitarSpecs falcon;
    falcon.manufacturer = "Gretsch";
    falcon.model = "White Falcon";
    falcon.series = "Professional Collection";
    falcon.yearIntroduced = 1954;
    falcon.description = "The ultimate rockabilly guitar with distinctive Gretsch tone";
    falcon.bodyStyle = GuitarBodyStyle::Hollow;
    falcon.bodyLength = 432;
    falcon.bodyWidth = 406;
    falcon.bodyThickness = 89;
    falcon.upperBoutWidth = 279;
    falcon.lowerBoutWidth = 406;
    falcon.waistWidth = 254;
    falcon.bodyWood = "Laminated Maple";
    falcon.bodyFinish = "White Lacquer";
    falcon.scaleLength = 628;
    falcon.neckLength = 628;
    falcon.numberOfFrets = 22;
    falcon.nutWidth = 43;
    falcon.neckThickness1stFret = 21;
    falcon.neckThickness12thFret = 24;
    falcon.neckWood = "Maple";
    falcon.fretboardWood = "Ebony";
    falcon.neckProfile = "Standard U";
    falcon.neckJoint = "Set Neck";
    falcon.headstockLength = 165;
    falcon.headstockWidth = 89;
    falcon.headstockStyle = "3+3";
    falcon.pickupConfig = PickupConfiguration::HH;
    falcon.pickupTypes = {"High Sensitive Filter'Tron", "High Sensitive Filter'Tron"};
    falcon.bridge = "Bigsby B6 Vibrato";
    falcon.tailpiece = "Bigsby";
    falcon.tuners = "Grover Imperial";
    falcon.numberOfStrings = 6;
    falcon.numberOfVolumes = 2;
    falcon.numberOfTones = 1;
    falcon.switchType = "3-Position Toggle";
    falcon.wiring = "Gretsch";
    falcon.specialFeatures = {"F-Holes", "Bigsby Vibrato", "Gold Hardware", "Falcon on Pickguard"};
    falcon.priceRange = "$3,000 - $5,000";
    m_specs.append(falcon);
}

void GuitarSpecsDatabase::addRickenbackerModels()
{
    // Rickenbacker 360
    GuitarSpecs rick360;
    rick360.manufacturer = "Rickenbacker";
    rick360.model = "360";
    rick360.series = "300";
    rick360.yearIntroduced = 1958;
    rick360.description = "Jangly tone and distinctive looks favored by The Beatles";
    rick360.bodyStyle = GuitarBodyStyle::SemiHollow;
    rick360.bodyLength = 432;
    rick360.bodyWidth = 343;
    rick360.bodyThickness = 38;
    rick360.upperBoutWidth = 254;
    rick360.lowerBoutWidth = 343;
    rick360.waistWidth = 216;
    rick360.bodyWood = "Maple";
    rick360.bodyFinish = "Lacquer";
    rick360.scaleLength = 629;
    rick360.neckLength = 629;
    rick360.numberOfFrets = 24;
    rick360.nutWidth = 41;
    rick360.neckThickness1stFret = 20;
    rick360.neckThickness12thFret = 22;
    rick360.neckWood = "Maple";
    rick360.fretboardWood = "Rosewood";
    rick360.neckProfile = "C";
    rick360.neckJoint = "Neck-Through";
    rick360.headstockLength = 140;
    rick360.headstockWidth = 89;
    rick360.headstockStyle = "6-in-line";
    rick360.pickupConfig = PickupConfiguration::HH;
    rick360.pickupTypes = {"Hi-Gain Single-Coil", "Hi-Gain Single-Coil"};
    rick360.bridge = "R Tailpiece";
    rick360.tailpiece = "R Tailpiece";
    rick360.tuners = "Schaller";
    rick360.numberOfStrings = 6;
    rick360.numberOfVolumes = 2;
    rick360.numberOfTones = 2;
    rick360.switchType = "3-Position Toggle";
    rick360.wiring = "Rickenbacker";
    rick360.specialFeatures = {"F-Holes", "Binding", "Dot Inlays", "Jangly Tone"};
    rick360.priceRange = "$2,200 - $3,000";
    m_specs.append(rick360);
}

// Utility methods implementation
QVector<GuitarSpecs> GuitarSpecsDatabase::getAllSpecs() const
{
    return m_specs;
}

QVector<GuitarSpecs> GuitarSpecsDatabase::getSpecsByManufacturer(GuitarManufacturer manufacturer) const
{
    return m_specsByManufacturer.value(manufacturer, QVector<GuitarSpecs>());
}

QVector<GuitarSpecs> GuitarSpecsDatabase::getSpecsByModel(const QString& model) const
{
    QVector<GuitarSpecs> results;
    for (const auto& spec : m_specs) {
        if (spec.model.contains(model, Qt::CaseInsensitive)) {
            results.append(spec);
        }
    }
    return results;
}

QVector<GuitarSpecs> GuitarSpecsDatabase::searchSpecs(const QString& searchTerm) const
{
    QVector<GuitarSpecs> results;
    QString term = searchTerm.toLower();
    
    for (const auto& spec : m_specs) {
        if (spec.manufacturer.toLower().contains(term) ||
            spec.model.toLower().contains(term) ||
            spec.series.toLower().contains(term) ||
            spec.description.toLower().contains(term) ||
            spec.bodyWood.toLower().contains(term) ||
            spec.neckWood.toLower().contains(term)) {
            results.append(spec);
        }
    }
    return results;
}

QStringList GuitarSpecsDatabase::getAllManufacturers() const
{
    QStringList manufacturers;
    for (auto it = m_specsByManufacturer.constBegin(); it != m_specsByManufacturer.constEnd(); ++it) {
        manufacturers.append(manufacturerToString(it.key()));
    }
    return manufacturers;
}

QStringList GuitarSpecsDatabase::getModelsByManufacturer(GuitarManufacturer manufacturer) const
{
    QStringList models;
    QVector<GuitarSpecs> specs = getSpecsByManufacturer(manufacturer);
    for (const auto& spec : specs) {
        models.append(spec.model);
    }
    return models;
}

QString GuitarSpecsDatabase::manufacturerToString(GuitarManufacturer manufacturer) const
{
    switch (manufacturer) {
        case GuitarManufacturer::Fender: return "Fender";
        case GuitarManufacturer::Gibson: return "Gibson";
        case GuitarManufacturer::PRS: return "PRS";
        case GuitarManufacturer::Ibanez: return "Ibanez";
        case GuitarManufacturer::ESP: return "ESP";
        case GuitarManufacturer::Jackson: return "Jackson";
        case GuitarManufacturer::Schecter: return "Schecter";
        case GuitarManufacturer::Epiphone: return "Epiphone";
        case GuitarManufacturer::Squier: return "Squier";
        case GuitarManufacturer::Martin: return "Martin";
        case GuitarManufacturer::Taylor: return "Taylor";
        case GuitarManufacturer::Yamaha: return "Yamaha";
        case GuitarManufacturer::Gretsch: return "Gretsch";
        case GuitarManufacturer::Rickenbacker: return "Rickenbacker";
        case GuitarManufacturer::MusicMan: return "Music Man";
        case GuitarManufacturer::Suhr: return "Suhr";
        case GuitarManufacturer::Charvel: return "Charvel";
        case GuitarManufacturer::Kramer: return "Kramer";
        case GuitarManufacturer::BCRich: return "BC Rich";
        case GuitarManufacturer::Dean: return "Dean";
        case GuitarManufacturer::Washburn: return "Washburn";
        case GuitarManufacturer::Ovation: return "Ovation";
        case GuitarManufacturer::Godin: return "Godin";
        case GuitarManufacturer::Seagull: return "Seagull";
        case GuitarManufacturer::Guild: return "Guild";
        default: return "Unknown";
    }
}

QString GuitarSpecsDatabase::bodyStyleToString(GuitarBodyStyle style) const
{
    switch (style) {
        case GuitarBodyStyle::SolidBody: return "Solid Body";
        case GuitarBodyStyle::SemiHollow: return "Semi-Hollow";
        case GuitarBodyStyle::Hollow: return "Hollow Body";
        case GuitarBodyStyle::HollowBody: return "Hollow Body";
        case GuitarBodyStyle::Chambered: return "Chambered";
        default: return "Unknown";
    }
}

QString GuitarSpecsDatabase::pickupConfigToString(PickupConfiguration config) const
{
    switch (config) {
        case PickupConfiguration::SSS: return "SSS (3 Single-Coils)";
        case PickupConfiguration::HSS: return "HSS (Humbucker + 2 Single-Coils)";
        case PickupConfiguration::HSH: return "HSH (Humbucker + Single + Humbucker)";
        case PickupConfiguration::HH: return "HH (2 Humbuckers)";
        case PickupConfiguration::HHH: return "HHH (3 Humbuckers)";
        case PickupConfiguration::P90_P90: return "P90 + P90";
        case PickupConfiguration::Single: return "Single Pickup";
        case PickupConfiguration::SS: return "SS (2 Single-Coils)";
        case PickupConfiguration::P: return "P (Single P90)";
        case PickupConfiguration::JJ: return "JJ (2 Jazz Pickups)";
        default: return "Unknown";
    }
}

GuitarSpecs GuitarSpecsDatabase::getSpecsByManufacturerAndModel(GuitarManufacturer manufacturer, const QString& model) const
{
    QVector<GuitarSpecs> specs = getSpecsByManufacturer(manufacturer);
    for (const auto& spec : specs) {
        if (spec.model == model) {
            return spec;
        }
    }
    return GuitarSpecs(); // Return empty specs if not found
}

// Missing manufacturer implementations
void GuitarSpecsDatabase::addMusicManModels()
{
    GuitarSpecs stingray;
    stingray.manufacturer = "Music Man";
    stingray.model = "StingRay";
    stingray.series = "Classic";
    stingray.yearIntroduced = 1976;
    stingray.description = "High-output bass with distinctive tone";
    stingray.bodyStyle = GuitarBodyStyle::SolidBody;
    stingray.bodyLength = 1143;
    stingray.bodyWidth = 330;
    stingray.bodyThickness = 45;
    stingray.scaleLength = 864;
    stingray.numberOfFrets = 21;
    stingray.nutWidth = 43;
    stingray.neckWood = "Maple";
    stingray.fretboardWood = "Maple";
    stingray.pickupConfig = PickupConfiguration::Single;
    stingray.numberOfStrings = 4;
    m_specs.append(stingray);
}

void GuitarSpecsDatabase::addSuhrModels()
{
    GuitarSpecs suhrClassic;
    suhrClassic.manufacturer = "Suhr";
    suhrClassic.model = "Classic S";
    suhrClassic.series = "Classic";
    suhrClassic.yearIntroduced = 1997;
    suhrClassic.description = "Boutique Stratocaster-style guitar";
    suhrClassic.bodyStyle = GuitarBodyStyle::SolidBody;
    suhrClassic.scaleLength = 648;
    suhrClassic.pickupConfig = PickupConfiguration::SSS;
    suhrClassic.numberOfStrings = 6;
    m_specs.append(suhrClassic);
}

void GuitarSpecsDatabase::addCharvelModels()
{
    GuitarSpecs charvelDK24;
    charvelDK24.manufacturer = "Charvel";
    charvelDK24.model = "DK24";
    charvelDK24.series = "Pro-Mod";
    charvelDK24.yearIntroduced = 2014;
    charvelDK24.description = "Modern high-performance guitar";
    charvelDK24.bodyStyle = GuitarBodyStyle::SolidBody;
    charvelDK24.scaleLength = 648;
    charvelDK24.pickupConfig = PickupConfiguration::HH;
    charvelDK24.numberOfStrings = 6;
    m_specs.append(charvelDK24);
}

void GuitarSpecsDatabase::addKramerModels()
{
    GuitarSpecs kramerBaretta;
    kramerBaretta.manufacturer = "Kramer";
    kramerBaretta.model = "Baretta";
    kramerBaretta.series = "Original";
    kramerBaretta.yearIntroduced = 1984;
    kramerBaretta.description = "1980s metal guitar icon";
    kramerBaretta.bodyStyle = GuitarBodyStyle::SolidBody;
    kramerBaretta.scaleLength = 648;
    kramerBaretta.pickupConfig = PickupConfiguration::Single;
    kramerBaretta.numberOfStrings = 6;
    m_specs.append(kramerBaretta);
}

void GuitarSpecsDatabase::addBCRichModels()
{
    GuitarSpecs warlock;
    warlock.manufacturer = "BC Rich";
    warlock.model = "Warlock";
    warlock.series = "Standard";
    warlock.yearIntroduced = 1981;
    warlock.description = "Extreme metal guitar with aggressive styling";
    warlock.bodyStyle = GuitarBodyStyle::SolidBody;
    warlock.scaleLength = 648;
    warlock.pickupConfig = PickupConfiguration::HH;
    warlock.numberOfStrings = 6;
    m_specs.append(warlock);
}

void GuitarSpecsDatabase::addDeanModels()
{
    GuitarSpecs deanV;
    deanV.manufacturer = "Dean";
    deanV.model = "V";
    deanV.series = "Standard";
    deanV.yearIntroduced = 1977;
    deanV.description = "Flying V style guitar";
    deanV.bodyStyle = GuitarBodyStyle::SolidBody;
    deanV.scaleLength = 648;
    deanV.pickupConfig = PickupConfiguration::HH;
    deanV.numberOfStrings = 6;
    m_specs.append(deanV);
}

void GuitarSpecsDatabase::addWashburnModels()
{
    GuitarSpecs washburn;
    washburn.manufacturer = "Washburn";
    washburn.model = "Dime 333";
    washburn.series = "Dimebag";
    washburn.yearIntroduced = 2004;
    washburn.description = "Dimebag Darrell signature model";
    washburn.bodyStyle = GuitarBodyStyle::SolidBody;
    washburn.scaleLength = 648;
    washburn.pickupConfig = PickupConfiguration::HH;
    washburn.numberOfStrings = 6;
    m_specs.append(washburn);
}

void GuitarSpecsDatabase::addOvationModels()
{
    GuitarSpecs ovation;
    ovation.manufacturer = "Ovation";
    ovation.model = "Celebrity";
    ovation.series = "Celebrity";
    ovation.yearIntroduced = 1966;
    ovation.description = "Round-back acoustic guitar";
    ovation.bodyStyle = GuitarBodyStyle::Hollow;
    ovation.scaleLength = 648;
    ovation.pickupConfig = PickupConfiguration::Single;
    ovation.numberOfStrings = 6;
    m_specs.append(ovation);
}

void GuitarSpecsDatabase::addGodinModels()
{
    GuitarSpecs godin;
    godin.manufacturer = "Godin";
    godin.model = "Session";
    godin.series = "Session";
    godin.yearIntroduced = 1987;
    godin.description = "Canadian-made versatile guitar";
    godin.bodyStyle = GuitarBodyStyle::SolidBody;
    godin.scaleLength = 648;
    godin.pickupConfig = PickupConfiguration::HSS;
    godin.numberOfStrings = 6;
    m_specs.append(godin);
}

void GuitarSpecsDatabase::addSeagullModels()
{
    GuitarSpecs seagull;
    seagull.manufacturer = "Seagull";
    seagull.model = "S6";
    seagull.series = "Original";
    seagull.yearIntroduced = 1982;
    seagull.description = "Canadian-made acoustic guitar";
    seagull.bodyStyle = GuitarBodyStyle::Hollow;
    seagull.scaleLength = 648;
    seagull.pickupConfig = PickupConfiguration::Single;
    seagull.numberOfStrings = 6;
    m_specs.append(seagull);
}

void GuitarSpecsDatabase::addGuildModels()
{
    GuitarSpecs guild;
    guild.manufacturer = "Guild";
    guild.model = "D-20";
    guild.series = "Traditional";
    guild.yearIntroduced = 1968;
    guild.description = "Traditional dreadnought acoustic";
    guild.bodyStyle = GuitarBodyStyle::Hollow;
    guild.scaleLength = 648;
    guild.pickupConfig = PickupConfiguration::Single;
    guild.numberOfStrings = 6;
    m_specs.append(guild);
}