#include "PhysicsCatalog.h"
#include <QSet>
#include <algorithm>

static QString S(const char *s) { return QString::fromUtf8(s); }

QVector<PhysicsConstant> PhysicsCatalog::constants()
{
    static QVector<PhysicsConstant> cache;
    if (!cache.isEmpty()) return cache;
    auto add = [&](const char *id, const char *name, const char *latex,
                   const char *unit, double value, const char *cat) {
        PhysicsConstant c;
        c.id = S(id); c.name = S(name); c.symbolLatex = S(latex);
        c.unit = S(unit); c.value = value; c.category = S(cat);
        cache.append(c);
    };
    add("c", "speed of light in vacuum", "c", "m/s", 299792458, "Universal");
    add("Gn", "Newtonian constant of gravitation", "G", "m^3/(kg·s^2)", 6.6743e-11, "Universal");
    add("h", "Planck constant", "h", "J·s", 6.62607015e-34, "Universal");
    add("hbar", "reduced Planck constant", "\\hbar", "J·s", 1.054571817e-34, "Universal");
    add("k", "Boltzmann constant", "k", "J/K", 1.380649e-23, "Universal");
    add("e_charge", "elementary charge", "e", "C", 1.602176634e-19, "Electromagnetic");
    add("eps0", "vacuum electric permittivity", "\\varepsilon_0", "F/m", 8.8541878128e-12, "Electromagnetic");
    add("mu0", "vacuum magnetic permeability", "\\mu_0", "N/A^2", 1.25663706212e-06, "Electromagnetic");
    add("Z0", "characteristic impedance of vacuum", "Z_0", "Ω", 376.730313668, "Electromagnetic");
    add("ke", "Coulomb constant", "k_e", "N·m^2/C^2", 8987551792.3, "Electromagnetic");
    add("alpha", "fine-structure constant", "\\alpha", "", 0.0072973525693, "Electromagnetic");
    add("NA", "Avogadro constant", "N_A", "1/mol", 6.02214076e+23, "Physico-chemical");
    add("R", "molar gas constant", "R", "J/(mol·K)", 8.314462618, "Physico-chemical");
    add("Faraday", "Faraday constant", "F", "C/mol", 96485.3321, "Physico-chemical");
    add("sigma", "Stefan–Boltzmann constant", "\\sigma", "W/(m^2·K^4)", 5.670374419e-08, "Universal");
    add("me", "electron mass", "m_e", "kg", 9.1093837015e-31, "Atomic");
    add("mp", "proton mass", "m_p", "kg", 1.67262192369e-27, "Atomic");
    add("mn", "neutron mass", "m_n", "kg", 1.67492749804e-27, "Atomic");
    add("u", "atomic mass constant", "u", "kg", 1.6605390666e-27, "Atomic");
    add("a0", "Bohr radius", "a_0", "m", 5.29177210903e-11, "Atomic");
    add("muB", "Bohr magneton", "\\mu_B", "J/T", 9.2740100783e-24, "Atomic");
    add("muN", "nuclear magneton", "\\mu_N", "J/T", 5.0507837461e-27, "Atomic");
    add("Rinf", "Rydberg constant", "R_\\infty", "1/m", 10973731.56816, "Atomic");
    add("eV", "electronvolt", "eV", "J", 1.602176634e-19, "Adopted");
    add("atm", "standard atmosphere", "atm", "Pa", 101325, "Adopted");
    add("g", "standard acceleration of gravity", "g", "m/s^2", 9.80665, "Adopted");
    add("Vm", "molar volume ideal gas (STP)", "V_m", "m^3/mol", 0.02241396954, "Physico-chemical");
    add("phi0", "magnetic flux quantum", "\\Phi_0", "Wb", 2.067833848e-15, "Electromagnetic");
    add("G0", "conductance quantum", "G_0", "S", 7.748091729e-05, "Electromagnetic");
    add("Rearth", "Earth mean radius", "R_E", "m", 6371000, "Astronomical");
    add("Mearth", "Earth mass", "M_E", "kg", 5.9722e+24, "Astronomical");
    add("Msun", "solar mass", "M_\\odot", "kg", 1.98847e+30, "Astronomical");
    add("AU", "astronomical unit", "au", "m", 149597870700, "Astronomical");
    add("ly", "light-year", "ly", "m", 9.4607304725808e+15, "Astronomical");
    add("pc", "parsec", "pc", "m", 3.08567758149137e+16, "Astronomical");
    add("cal", "thermochemical calorie", "cal", "J", 4.184, "Adopted");
    add("hp", "horsepower (metric)", "hp", "W", 735.49875, "Adopted");
    add("Torr", "torr", "Torr", "Pa", 133.322368421, "Adopted");
    add("amu", "unified atomic mass unit", "u", "kg", 1.6605390666e-27, "Atomic");
    add("lambdaC", "Compton wavelength (electron)", "\\lambda_C", "m", 2.42631023867e-12, "Atomic");
    add("re", "classical electron radius", "r_e", "m", 2.8179403262e-15, "Atomic");
    add("sigmaT", "Thomson cross section", "\\sigma_T", "m^2", 6.6524587321e-29, "Atomic");
    return cache;
}

QHash<QString, double> PhysicsCatalog::constantValues()
{
    QHash<QString, double> m;
    for (const auto &c : constants())
        m.insert(c.id.toLower(), c.value);
    // Aliases used in equations
    m.insert(QStringLiteral("ke"), m.value(QStringLiteral("ke")));
    return m;
}

static PhysicsVariable Var(const char *id, const char *name, const char *latex, const char *unit)
{
    PhysicsVariable v;
    v.id = S(id); v.name = S(name); v.symbolLatex = S(latex); v.unit = S(unit);
    return v;
}

QVector<PhysicsEquation> PhysicsCatalog::equations()
{
    static QVector<PhysicsEquation> cache;
    if (!cache.isEmpty()) return cache;
    auto add = [&](PhysicsEquation e) { cache.append(std::move(e)); };

    {
        PhysicsEquation e;
        e.id = S("kin_v");
        e.field = S("Mechanics — Kinematics");
        e.name = S("Velocity under constant acceleration");
        e.latex = S("v = v_0 + a t");
        e.description = S("Linear motion with constant a");
        e.defaultUnknown = S("v");
        e.variables.append(Var("v", "final velocity", "v", "m/s"));
        e.variables.append(Var("v0", "initial velocity", "v_0", "m/s"));
        e.variables.append(Var("a", "acceleration", "a", "m/s^2"));
        e.variables.append(Var("t", "time", "t", "s"));
        e.solve.insert(S("v"), S("v0+a*t"));
        e.solve.insert(S("v0"), S("v-a*t"));
        e.solve.insert(S("a"), S("(v-v0)/t"));
        e.solve.insert(S("t"), S("(v-v0)/a"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("kin_x");
        e.field = S("Mechanics — Kinematics");
        e.name = S("Displacement with constant acceleration");
        e.latex = S("x = x_0 + v_0 t + \\frac{1}{2} a t^2");
        e.description = S("Position vs time");
        e.defaultUnknown = S("x");
        e.variables.append(Var("x", "final position", "x", "m"));
        e.variables.append(Var("x0", "initial position", "x_0", "m"));
        e.variables.append(Var("v0", "initial velocity", "v_0", "m/s"));
        e.variables.append(Var("a", "acceleration", "a", "m/s^2"));
        e.variables.append(Var("t", "time", "t", "s"));
        e.solve.insert(S("x"), S("x0+v0*t+0.5*a*t^2"));
        e.solve.insert(S("x0"), S("x-v0*t-0.5*a*t^2"));
        e.solve.insert(S("a"), S("2*(x-x0-v0*t)/(t^2)"));
        e.solve.insert(S("v0"), S("(x-x0-0.5*a*t^2)/t"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("kin_v2");
        e.field = S("Mechanics — Kinematics");
        e.name = S("Velocity–displacement relation");
        e.latex = S("v^2 = v_0^2 + 2 a s");
        e.description = S("Eliminate time");
        e.defaultUnknown = S("v");
        e.variables.append(Var("v", "final speed", "v", "m/s"));
        e.variables.append(Var("v0", "initial speed", "v_0", "m/s"));
        e.variables.append(Var("a", "acceleration", "a", "m/s^2"));
        e.variables.append(Var("s", "displacement", "s", "m"));
        e.solve.insert(S("v"), S("sqrt(v0^2+2*a*s)"));
        e.solve.insert(S("v0"), S("sqrt(v^2-2*a*s)"));
        e.solve.insert(S("a"), S("(v^2-v0^2)/(2*s)"));
        e.solve.insert(S("s"), S("(v^2-v0^2)/(2*a)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("kin_avg");
        e.field = S("Mechanics — Kinematics");
        e.name = S("Average velocity");
        e.latex = S("v_{avg} = \\frac{v + v_0}{2}");
        e.description = S("Constant acceleration average");
        e.defaultUnknown = S("vavg");
        e.variables.append(Var("vavg", "average velocity", "v_{avg}", "m/s"));
        e.variables.append(Var("v", "final velocity", "v", "m/s"));
        e.variables.append(Var("v0", "initial velocity", "v_0", "m/s"));
        e.solve.insert(S("vavg"), S("(v+v0)/2"));
        e.solve.insert(S("v"), S("2*vavg-v0"));
        e.solve.insert(S("v0"), S("2*vavg-v"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("dyn_F");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Newton's second law");
        e.latex = S("F = m a");
        e.description = S("Net force");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "net force", "F", "N"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("a", "acceleration", "a", "m/s^2"));
        e.solve.insert(S("F"), S("m*a"));
        e.solve.insert(S("m"), S("F/a"));
        e.solve.insert(S("a"), S("F/m"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("dyn_W");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Work (constant force, collinear)");
        e.latex = S("W = F d");
        e.description = S("Work along displacement");
        e.defaultUnknown = S("W");
        e.variables.append(Var("W", "work", "W", "J"));
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("d", "displacement", "d", "m"));
        e.solve.insert(S("W"), S("F*d"));
        e.solve.insert(S("F"), S("W/d"));
        e.solve.insert(S("d"), S("W/F"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("dyn_P");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Power");
        e.latex = S("P = F v");
        e.description = S("Instantaneous power");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "power", "P", "W"));
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("v", "velocity", "v", "m/s"));
        e.solve.insert(S("P"), S("F*v"));
        e.solve.insert(S("F"), S("P/v"));
        e.solve.insert(S("v"), S("P/F"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("dyn_fric");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Kinetic friction");
        e.latex = S("f_k = \\mu_k N");
        e.description = S("Friction magnitude");
        e.defaultUnknown = S("fk");
        e.variables.append(Var("fk", "friction force", "f_k", "N"));
        e.variables.append(Var("muk", "kinetic friction coefficient", "\\mu_k", ""));
        e.variables.append(Var("N", "normal force", "N", "N"));
        e.solve.insert(S("fk"), S("muk*N"));
        e.solve.insert(S("muk"), S("fk/N"));
        e.solve.insert(S("N"), S("fk/muk"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("dyn_cent");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Centripetal force");
        e.latex = S("F_c = \\frac{m v^2}{r}");
        e.description = S("Uniform circular motion");
        e.defaultUnknown = S("Fc");
        e.variables.append(Var("Fc", "centripetal force", "F_c", "N"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.variables.append(Var("r", "radius", "r", "m"));
        e.solve.insert(S("Fc"), S("m*v^2/r"));
        e.solve.insert(S("m"), S("Fc*r/v^2"));
        e.solve.insert(S("v"), S("sqrt(Fc*r/m)"));
        e.solve.insert(S("r"), S("m*v^2/Fc"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("en_K");
        e.field = S("Mechanics — Energy");
        e.name = S("Kinetic energy");
        e.latex = S("K = \\frac{1}{2} m v^2");
        e.description = S("Translational KE");
        e.defaultUnknown = S("K");
        e.variables.append(Var("K", "kinetic energy", "K", "J"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.solve.insert(S("K"), S("0.5*m*v^2"));
        e.solve.insert(S("m"), S("2*K/v^2"));
        e.solve.insert(S("v"), S("sqrt(2*K/m)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("en_U");
        e.field = S("Mechanics — Energy");
        e.name = S("Gravitational potential energy (near Earth)");
        e.latex = S("U = m g h");
        e.description = S("Near-surface PE");
        e.defaultUnknown = S("U");
        e.variables.append(Var("U", "potential energy", "U", "J"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("g", "gravity", "g", "m/s^2"));
        e.variables.append(Var("h", "height", "h", "m"));
        e.solve.insert(S("U"), S("m*g*h"));
        e.solve.insert(S("m"), S("U/(g*h)"));
        e.solve.insert(S("h"), S("U/(m*g)"));
        e.solve.insert(S("g"), S("U/(m*h)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("en_spring");
        e.field = S("Mechanics — Energy");
        e.name = S("Elastic potential energy");
        e.latex = S("U_s = \\frac{1}{2} k x^2");
        e.description = S("Hooke spring");
        e.defaultUnknown = S("Us");
        e.variables.append(Var("Us", "spring energy", "U_s", "J"));
        e.variables.append(Var("k", "spring constant", "k", "N/m"));
        e.variables.append(Var("x", "extension", "x", "m"));
        e.solve.insert(S("Us"), S("0.5*k*x^2"));
        e.solve.insert(S("k"), S("2*Us/x^2"));
        e.solve.insert(S("x"), S("sqrt(2*Us/k)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("en_Wnet");
        e.field = S("Mechanics — Energy");
        e.name = S("Work–energy theorem");
        e.latex = S("W_{net} = K_f - K_i");
        e.description = S("Net work equals ΔK");
        e.defaultUnknown = S("W");
        e.variables.append(Var("W", "net work", "W_{net}", "J"));
        e.variables.append(Var("Kf", "final KE", "K_f", "J"));
        e.variables.append(Var("Ki", "initial KE", "K_i", "J"));
        e.solve.insert(S("W"), S("Kf-Ki"));
        e.solve.insert(S("Kf"), S("Ki+W"));
        e.solve.insert(S("Ki"), S("Kf-W"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mom_p");
        e.field = S("Mechanics — Momentum");
        e.name = S("Linear momentum");
        e.latex = S("p = m v");
        e.description = S("Momentum");
        e.defaultUnknown = S("p");
        e.variables.append(Var("p", "momentum", "p", "kg·m/s"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("v", "velocity", "v", "m/s"));
        e.solve.insert(S("p"), S("m*v"));
        e.solve.insert(S("m"), S("p/v"));
        e.solve.insert(S("v"), S("p/m"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mom_J");
        e.field = S("Mechanics — Momentum");
        e.name = S("Impulse");
        e.latex = S("J = F \\Delta t");
        e.description = S("Impulse = change in momentum");
        e.defaultUnknown = S("J");
        e.variables.append(Var("J", "impulse", "J", "N·s"));
        e.variables.append(Var("F", "average force", "F", "N"));
        e.variables.append(Var("dt", "time interval", "\\Delta t", "s"));
        e.solve.insert(S("J"), S("F*dt"));
        e.solve.insert(S("F"), S("J/dt"));
        e.solve.insert(S("dt"), S("J/F"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rot_omega");
        e.field = S("Mechanics — Rotation");
        e.name = S("Angular velocity under constant α");
        e.latex = S("\\omega = \\omega_0 + \\alpha t");
        e.description = S("Rotational kinematics");
        e.defaultUnknown = S("omega");
        e.variables.append(Var("omega", "final angular velocity", "\\omega", "rad/s"));
        e.variables.append(Var("omega0", "initial angular velocity", "\\omega_0", "rad/s"));
        e.variables.append(Var("alpha", "angular acceleration", "\\alpha", "rad/s^2"));
        e.variables.append(Var("t", "time", "t", "s"));
        e.solve.insert(S("omega"), S("omega0+alpha*t"));
        e.solve.insert(S("omega0"), S("omega-alpha*t"));
        e.solve.insert(S("alpha"), S("(omega-omega0)/t"));
        e.solve.insert(S("t"), S("(omega-omega0)/alpha"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rot_tau");
        e.field = S("Mechanics — Rotation");
        e.name = S("Newton's second law for rotation");
        e.latex = S("\\tau = I \\alpha");
        e.description = S("Torque");
        e.defaultUnknown = S("tau");
        e.variables.append(Var("tau", "torque", "\\tau", "N·m"));
        e.variables.append(Var("I", "moment of inertia", "I", "kg·m^2"));
        e.variables.append(Var("alpha", "angular acceleration", "\\alpha", "rad/s^2"));
        e.solve.insert(S("tau"), S("I*alpha"));
        e.solve.insert(S("I"), S("tau/alpha"));
        e.solve.insert(S("alpha"), S("tau/I"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rot_L");
        e.field = S("Mechanics — Rotation");
        e.name = S("Angular momentum");
        e.latex = S("L = I \\omega");
        e.description = S("Angular momentum");
        e.defaultUnknown = S("L");
        e.variables.append(Var("L", "angular momentum", "L", "kg·m^2/s"));
        e.variables.append(Var("I", "moment of inertia", "I", "kg·m^2"));
        e.variables.append(Var("omega", "angular velocity", "\\omega", "rad/s"));
        e.solve.insert(S("L"), S("I*omega"));
        e.solve.insert(S("I"), S("L/omega"));
        e.solve.insert(S("omega"), S("L/I"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rot_K");
        e.field = S("Mechanics — Rotation");
        e.name = S("Rotational kinetic energy");
        e.latex = S("K = \\frac{1}{2} I \\omega^2");
        e.description = S("Rotational KE");
        e.defaultUnknown = S("K");
        e.variables.append(Var("K", "kinetic energy", "K", "J"));
        e.variables.append(Var("I", "moment of inertia", "I", "kg·m^2"));
        e.variables.append(Var("omega", "angular velocity", "\\omega", "rad/s"));
        e.solve.insert(S("K"), S("0.5*I*omega^2"));
        e.solve.insert(S("I"), S("2*K/omega^2"));
        e.solve.insert(S("omega"), S("sqrt(2*K/I)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("grav_F");
        e.field = S("Mechanics — Gravitation");
        e.name = S("Newton's law of gravitation");
        e.latex = S("F = G \\frac{m_1 m_2}{r^2}");
        e.description = S("Mutual gravity");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("m1", "mass 1", "m_1", "kg"));
        e.variables.append(Var("m2", "mass 2", "m_2", "kg"));
        e.variables.append(Var("r", "separation", "r", "m"));
        e.variables.append(Var("Gn", "gravitational constant", "G", "m^3/(kg·s^2)"));
        e.solve.insert(S("F"), S("Gn*m1*m2/r^2"));
        e.solve.insert(S("m1"), S("F*r^2/(Gn*m2)"));
        e.solve.insert(S("m2"), S("F*r^2/(Gn*m1)"));
        e.solve.insert(S("r"), S("sqrt(Gn*m1*m2/F)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("grav_g");
        e.field = S("Mechanics — Gravitation");
        e.name = S("Gravitational field");
        e.latex = S("g = G \\frac{M}{r^2}");
        e.description = S("g at distance r from mass M");
        e.defaultUnknown = S("g");
        e.variables.append(Var("g", "field strength", "g", "m/s^2"));
        e.variables.append(Var("M", "source mass", "M", "kg"));
        e.variables.append(Var("r", "distance", "r", "m"));
        e.variables.append(Var("Gn", "gravitational constant", "G", "m^3/(kg·s^2)"));
        e.solve.insert(S("g"), S("Gn*M/r^2"));
        e.solve.insert(S("M"), S("g*r^2/Gn"));
        e.solve.insert(S("r"), S("sqrt(Gn*M/g)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("grav_vorb");
        e.field = S("Mechanics — Gravitation");
        e.name = S("Circular orbit speed");
        e.latex = S("v = \\sqrt{\\frac{G M}{r}}");
        e.description = S("Orbital speed");
        e.defaultUnknown = S("v");
        e.variables.append(Var("v", "orbital speed", "v", "m/s"));
        e.variables.append(Var("M", "central mass", "M", "kg"));
        e.variables.append(Var("r", "orbit radius", "r", "m"));
        e.variables.append(Var("Gn", "gravitational constant", "G", "m^3/(kg·s^2)"));
        e.solve.insert(S("v"), S("sqrt(Gn*M/r)"));
        e.solve.insert(S("M"), S("v^2*r/Gn"));
        e.solve.insert(S("r"), S("Gn*M/v^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("grav_T");
        e.field = S("Mechanics — Gravitation");
        e.name = S("Kepler period (circular)");
        e.latex = S("T = 2\\pi \\sqrt{\\frac{r^3}{G M}}");
        e.description = S("Orbital period");
        e.defaultUnknown = S("T");
        e.variables.append(Var("T", "period", "T", "s"));
        e.variables.append(Var("r", "semi-major/radius", "r", "m"));
        e.variables.append(Var("M", "central mass", "M", "kg"));
        e.variables.append(Var("Gn", "gravitational constant", "G", "m^3/(kg·s^2)"));
        e.solve.insert(S("T"), S("2*pi*sqrt(r^3/(Gn*M))"));
        e.solve.insert(S("M"), S("4*pi^2*r^3/(Gn*T^2)"));
        e.solve.insert(S("r"), S("(Gn*M*T^2/(4*pi^2))^(1/3)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("grav_esc");
        e.field = S("Mechanics — Gravitation");
        e.name = S("Escape velocity");
        e.latex = S("v_{esc} = \\sqrt{\\frac{2 G M}{r}}");
        e.description = S("Escape speed");
        e.defaultUnknown = S("vesc");
        e.variables.append(Var("vesc", "escape speed", "v_{esc}", "m/s"));
        e.variables.append(Var("M", "mass", "M", "kg"));
        e.variables.append(Var("r", "radius", "r", "m"));
        e.variables.append(Var("Gn", "gravitational constant", "G", "m^3/(kg·s^2)"));
        e.solve.insert(S("vesc"), S("sqrt(2*Gn*M/r)"));
        e.solve.insert(S("M"), S("vesc^2*r/(2*Gn)"));
        e.solve.insert(S("r"), S("2*Gn*M/vesc^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("osc_spring");
        e.field = S("Waves & Oscillations");
        e.name = S("Mass–spring period");
        e.latex = S("T = 2\\pi \\sqrt{\\frac{m}{k}}");
        e.description = S("Simple harmonic motion");
        e.defaultUnknown = S("T");
        e.variables.append(Var("T", "period", "T", "s"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("k", "spring constant", "k", "N/m"));
        e.solve.insert(S("T"), S("2*pi*sqrt(m/k)"));
        e.solve.insert(S("m"), S("k*T^2/(4*pi^2)"));
        e.solve.insert(S("k"), S("4*pi^2*m/T^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("osc_pend");
        e.field = S("Waves & Oscillations");
        e.name = S("Simple pendulum period");
        e.latex = S("T = 2\\pi \\sqrt{\\frac{L}{g}}");
        e.description = S("Small-angle pendulum");
        e.defaultUnknown = S("T");
        e.variables.append(Var("T", "period", "T", "s"));
        e.variables.append(Var("L", "length", "L", "m"));
        e.variables.append(Var("g", "gravity", "g", "m/s^2"));
        e.solve.insert(S("T"), S("2*pi*sqrt(L/g)"));
        e.solve.insert(S("L"), S("g*T^2/(4*pi^2)"));
        e.solve.insert(S("g"), S("4*pi^2*L/T^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("osc_omega");
        e.field = S("Waves & Oscillations");
        e.name = S("Angular frequency (spring)");
        e.latex = S("\\omega = \\sqrt{\\frac{k}{m}}");
        e.description = S("Natural angular frequency");
        e.defaultUnknown = S("omega");
        e.variables.append(Var("omega", "angular frequency", "\\omega", "rad/s"));
        e.variables.append(Var("k", "spring constant", "k", "N/m"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.solve.insert(S("omega"), S("sqrt(k/m)"));
        e.solve.insert(S("k"), S("omega^2*m"));
        e.solve.insert(S("m"), S("k/omega^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("wave_v");
        e.field = S("Waves & Oscillations");
        e.name = S("Wave speed");
        e.latex = S("v = f \\lambda");
        e.description = S("Wave relation");
        e.defaultUnknown = S("v");
        e.variables.append(Var("v", "wave speed", "v", "m/s"));
        e.variables.append(Var("f", "frequency", "f", "Hz"));
        e.variables.append(Var("lambda", "wavelength", "\\lambda", "m"));
        e.solve.insert(S("v"), S("f*lambda"));
        e.solve.insert(S("f"), S("v/lambda"));
        e.solve.insert(S("lambda"), S("v/f"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("wave_T");
        e.field = S("Waves & Oscillations");
        e.name = S("Period–frequency");
        e.latex = S("T = \\frac{1}{f}");
        e.description = S("Period");
        e.defaultUnknown = S("T");
        e.variables.append(Var("T", "period", "T", "s"));
        e.variables.append(Var("f", "frequency", "f", "Hz"));
        e.solve.insert(S("T"), S("1/f"));
        e.solve.insert(S("f"), S("1/T"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("wave_string");
        e.field = S("Waves & Oscillations");
        e.name = S("Wave on a string");
        e.latex = S("v = \\sqrt{\\frac{T_s}{\\mu}}");
        e.description = S("Transverse wave speed");
        e.defaultUnknown = S("v");
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.variables.append(Var("Ts", "tension", "T_s", "N"));
        e.variables.append(Var("mu", "linear density", "\\mu", "kg/m"));
        e.solve.insert(S("v"), S("sqrt(Ts/mu)"));
        e.solve.insert(S("Ts"), S("v^2*mu"));
        e.solve.insert(S("mu"), S("Ts/v^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("flu_P");
        e.field = S("Fluids");
        e.name = S("Hydrostatic pressure");
        e.latex = S("P = \\rho g h");
        e.description = S("Gauge pressure depth h");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "pressure", "P", "Pa"));
        e.variables.append(Var("rho", "density", "\\rho", "kg/m^3"));
        e.variables.append(Var("g", "gravity", "g", "m/s^2"));
        e.variables.append(Var("h", "depth", "h", "m"));
        e.solve.insert(S("P"), S("rho*g*h"));
        e.solve.insert(S("rho"), S("P/(g*h)"));
        e.solve.insert(S("h"), S("P/(rho*g)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("flu_cont");
        e.field = S("Fluids");
        e.name = S("Continuity equation");
        e.latex = S("A_1 v_1 = A_2 v_2");
        e.description = S("Incompressible flow");
        e.defaultUnknown = S("v2");
        e.variables.append(Var("A1", "area 1", "A_1", "m^2"));
        e.variables.append(Var("v1", "speed 1", "v_1", "m/s"));
        e.variables.append(Var("A2", "area 2", "A_2", "m^2"));
        e.variables.append(Var("v2", "speed 2", "v_2", "m/s"));
        e.solve.insert(S("v2"), S("A1*v1/A2"));
        e.solve.insert(S("v1"), S("A2*v2/A1"));
        e.solve.insert(S("A2"), S("A1*v1/v2"));
        e.solve.insert(S("A1"), S("A2*v2/v1"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("flu_Q");
        e.field = S("Fluids");
        e.name = S("Volume flow rate");
        e.latex = S("Q = A v");
        e.description = S("Flow rate");
        e.defaultUnknown = S("Q");
        e.variables.append(Var("Q", "flow rate", "Q", "m^3/s"));
        e.variables.append(Var("A", "area", "A", "m^2"));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.solve.insert(S("Q"), S("A*v"));
        e.solve.insert(S("A"), S("Q/v"));
        e.solve.insert(S("v"), S("Q/A"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_idealgas");
        e.field = S("Thermodynamics");
        e.name = S("Ideal gas law");
        e.latex = S("P V = n R T");
        e.description = S("Ideal gas");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "pressure", "P", "Pa"));
        e.variables.append(Var("V", "volume", "V", "m^3"));
        e.variables.append(Var("n", "amount", "n", "mol"));
        e.variables.append(Var("R", "gas constant", "R", "J/(mol·K)"));
        e.variables.append(Var("T", "temperature", "T", "K"));
        e.solve.insert(S("P"), S("n*R*T/V"));
        e.solve.insert(S("V"), S("n*R*T/P"));
        e.solve.insert(S("n"), S("P*V/(R*T)"));
        e.solve.insert(S("T"), S("P*V/(n*R)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_Q");
        e.field = S("Thermodynamics");
        e.name = S("Heat capacity (no phase change)");
        e.latex = S("Q = m c \\Delta T");
        e.description = S("Sensible heat");
        e.defaultUnknown = S("Q");
        e.variables.append(Var("Q", "heat", "Q", "J"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("c", "specific heat", "c", "J/(kg·K)"));
        e.variables.append(Var("dT", "temperature change", "\\Delta T", "K"));
        e.solve.insert(S("Q"), S("m*c*dT"));
        e.solve.insert(S("m"), S("Q/(c*dT)"));
        e.solve.insert(S("c"), S("Q/(m*dT)"));
        e.solve.insert(S("dT"), S("Q/(m*c)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_latent");
        e.field = S("Thermodynamics");
        e.name = S("Latent heat");
        e.latex = S("Q = m L");
        e.description = S("Phase-change heat");
        e.defaultUnknown = S("Q");
        e.variables.append(Var("Q", "heat", "Q", "J"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("L", "latent heat", "L", "J/kg"));
        e.solve.insert(S("Q"), S("m*L"));
        e.solve.insert(S("m"), S("Q/L"));
        e.solve.insert(S("L"), S("Q/m"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_carnot");
        e.field = S("Thermodynamics");
        e.name = S("Carnot efficiency");
        e.latex = S("\\eta = 1 - \\frac{T_c}{T_h}");
        e.description = S("Maximum heat-engine efficiency");
        e.defaultUnknown = S("eta");
        e.variables.append(Var("eta", "efficiency", "\\eta", ""));
        e.variables.append(Var("Tc", "cold reservoir", "T_c", "K"));
        e.variables.append(Var("Th", "hot reservoir", "T_h", "K"));
        e.solve.insert(S("eta"), S("1-Tc/Th"));
        e.solve.insert(S("Tc"), S("Th*(1-eta)"));
        e.solve.insert(S("Th"), S("Tc/(1-eta)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_KE");
        e.field = S("Thermodynamics");
        e.name = S("Average molecular KE (3D)");
        e.latex = S("K_{avg} = \\frac{3}{2} k T");
        e.description = S("Translational KE per molecule");
        e.defaultUnknown = S("Kavg");
        e.variables.append(Var("Kavg", "average KE", "K_{avg}", "J"));
        e.variables.append(Var("k", "Boltzmann", "k", "J/K"));
        e.variables.append(Var("T", "temperature", "T", "K"));
        e.solve.insert(S("Kavg"), S("1.5*k*T"));
        e.solve.insert(S("T"), S("Kavg/(1.5*k)"));
        e.solve.insert(S("k"), S("Kavg/(1.5*T)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_F");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Coulomb's law");
        e.latex = S("F = k_e \\frac{q_1 q_2}{r^2}");
        e.description = S("Force between point charges");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("q1", "charge 1", "q_1", "C"));
        e.variables.append(Var("q2", "charge 2", "q_2", "C"));
        e.variables.append(Var("r", "separation", "r", "m"));
        e.variables.append(Var("ke", "Coulomb constant", "k_e", "N·m^2/C^2"));
        e.solve.insert(S("F"), S("ke*q1*q2/r^2"));
        e.solve.insert(S("q1"), S("F*r^2/(ke*q2)"));
        e.solve.insert(S("q2"), S("F*r^2/(ke*q1)"));
        e.solve.insert(S("r"), S("sqrt(ke*q1*q2/F)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_E");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Electric field of point charge");
        e.latex = S("E = k_e \\frac{q}{r^2}");
        e.description = S("Field magnitude");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "electric field", "E", "N/C"));
        e.variables.append(Var("q", "charge", "q", "C"));
        e.variables.append(Var("r", "distance", "r", "m"));
        e.variables.append(Var("ke", "k_e", "k_e", "N·m^2/C^2"));
        e.solve.insert(S("E"), S("ke*q/r^2"));
        e.solve.insert(S("q"), S("E*r^2/ke"));
        e.solve.insert(S("r"), S("sqrt(ke*q/E)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_V");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Electric potential of point charge");
        e.latex = S("V = k_e \\frac{q}{r}");
        e.description = S("Potential (V=0 at ∞)");
        e.defaultUnknown = S("V");
        e.variables.append(Var("V", "potential", "V", "V"));
        e.variables.append(Var("q", "charge", "q", "C"));
        e.variables.append(Var("r", "distance", "r", "m"));
        e.variables.append(Var("ke", "k_e", "k_e", "N·m^2/C^2"));
        e.solve.insert(S("V"), S("ke*q/r"));
        e.solve.insert(S("q"), S("V*r/ke"));
        e.solve.insert(S("r"), S("ke*q/V"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_C");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Capacitance definition");
        e.latex = S("C = \\frac{Q}{V}");
        e.description = S("Capacitance");
        e.defaultUnknown = S("C");
        e.variables.append(Var("C", "capacitance", "C", "F"));
        e.variables.append(Var("Q", "charge", "Q", "C"));
        e.variables.append(Var("V", "voltage", "V", "V"));
        e.solve.insert(S("C"), S("Q/V"));
        e.solve.insert(S("Q"), S("C*V"));
        e.solve.insert(S("V"), S("Q/C"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_Uc");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Capacitor energy");
        e.latex = S("U = \\frac{1}{2} C V^2");
        e.description = S("Stored energy");
        e.defaultUnknown = S("U");
        e.variables.append(Var("U", "energy", "U", "J"));
        e.variables.append(Var("C", "capacitance", "C", "F"));
        e.variables.append(Var("V", "voltage", "V", "V"));
        e.solve.insert(S("U"), S("0.5*C*V^2"));
        e.solve.insert(S("C"), S("2*U/V^2"));
        e.solve.insert(S("V"), S("sqrt(2*U/C)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_Ohm");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Ohm's law");
        e.latex = S("V = I R");
        e.description = S("Resistor");
        e.defaultUnknown = S("V");
        e.variables.append(Var("V", "voltage", "V", "V"));
        e.variables.append(Var("I", "current", "I", "A"));
        e.variables.append(Var("R", "resistance", "R", "Ω"));
        e.solve.insert(S("V"), S("I*R"));
        e.solve.insert(S("I"), S("V/R"));
        e.solve.insert(S("R"), S("V/I"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_P");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Electric power");
        e.latex = S("P = I V");
        e.description = S("Power");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "power", "P", "W"));
        e.variables.append(Var("I", "current", "I", "A"));
        e.variables.append(Var("V", "voltage", "V", "V"));
        e.solve.insert(S("P"), S("I*V"));
        e.solve.insert(S("I"), S("P/V"));
        e.solve.insert(S("V"), S("P/I"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_P2");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Joule heating");
        e.latex = S("P = I^2 R");
        e.description = S("Power in resistor");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "power", "P", "W"));
        e.variables.append(Var("I", "current", "I", "A"));
        e.variables.append(Var("R", "resistance", "R", "Ω"));
        e.solve.insert(S("P"), S("I^2*R"));
        e.solve.insert(S("R"), S("P/I^2"));
        e.solve.insert(S("I"), S("sqrt(P/R)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_series");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Two resistors in series");
        e.latex = S("R = R_1 + R_2");
        e.description = S("Series equivalent");
        e.defaultUnknown = S("R");
        e.variables.append(Var("R", "equivalent", "R", "Ω"));
        e.variables.append(Var("R1", "resistor 1", "R_1", "Ω"));
        e.variables.append(Var("R2", "resistor 2", "R_2", "Ω"));
        e.solve.insert(S("R"), S("R1+R2"));
        e.solve.insert(S("R1"), S("R-R2"));
        e.solve.insert(S("R2"), S("R-R1"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_parallel");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Two resistors in parallel");
        e.latex = S("\\frac{1}{R} = \\frac{1}{R_1}+\\frac{1}{R_2}");
        e.description = S("Parallel equivalent");
        e.defaultUnknown = S("R");
        e.variables.append(Var("R", "equivalent", "R", "Ω"));
        e.variables.append(Var("R1", "resistor 1", "R_1", "Ω"));
        e.variables.append(Var("R2", "resistor 2", "R_2", "Ω"));
        e.solve.insert(S("R"), S("(R1*R2)/(R1+R2)"));
        e.solve.insert(S("R1"), S("(R*R2)/(R2-R)"));
        e.solve.insert(S("R2"), S("(R*R1)/(R1-R)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_tau");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("RC time constant");
        e.latex = S("\\tau = R C");
        e.description = S("Charging time constant");
        e.defaultUnknown = S("tau");
        e.variables.append(Var("tau", "time constant", "\\tau", "s"));
        e.variables.append(Var("R", "resistance", "R", "Ω"));
        e.variables.append(Var("C", "capacitance", "C", "F"));
        e.solve.insert(S("tau"), S("R*C"));
        e.solve.insert(S("R"), S("tau/C"));
        e.solve.insert(S("C"), S("tau/R"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mag_Fb");
        e.field = S("Electromagnetism — Magnetism");
        e.name = S("Magnetic force on moving charge (perp)");
        e.latex = S("F = q v B");
        e.description = S("v ⟂ B");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("q", "charge", "q", "C"));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.variables.append(Var("B", "magnetic field", "B", "T"));
        e.solve.insert(S("F"), S("q*v*B"));
        e.solve.insert(S("q"), S("F/(v*B)"));
        e.solve.insert(S("v"), S("F/(q*B)"));
        e.solve.insert(S("B"), S("F/(q*v)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mag_FI");
        e.field = S("Electromagnetism — Magnetism");
        e.name = S("Force on current-carrying wire (perp)");
        e.latex = S("F = I L B");
        e.description = S("I ⟂ B");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "force", "F", "N"));
        e.variables.append(Var("I", "current", "I", "A"));
        e.variables.append(Var("L", "length", "L", "m"));
        e.variables.append(Var("B", "magnetic field", "B", "T"));
        e.solve.insert(S("F"), S("I*L*B"));
        e.solve.insert(S("I"), S("F/(L*B)"));
        e.solve.insert(S("L"), S("F/(I*B)"));
        e.solve.insert(S("B"), S("F/(I*L)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mag_Bwire");
        e.field = S("Electromagnetism — Magnetism");
        e.name = S("B field of long straight wire");
        e.latex = S("B = \\frac{\\mu_0 I}{2\\pi r}");
        e.description = S("Ampère");
        e.defaultUnknown = S("B");
        e.variables.append(Var("B", "magnetic field", "B", "T"));
        e.variables.append(Var("I", "current", "I", "A"));
        e.variables.append(Var("r", "distance", "r", "m"));
        e.variables.append(Var("mu0", "μ₀", "\\mu_0", "N/A^2"));
        e.solve.insert(S("B"), S("mu0*I/(2*pi*r)"));
        e.solve.insert(S("I"), S("B*2*pi*r/mu0"));
        e.solve.insert(S("r"), S("mu0*I/(2*pi*B)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mag_Phi");
        e.field = S("Electromagnetism — Magnetism");
        e.name = S("Magnetic flux (uniform, perp)");
        e.latex = S("\\Phi = B A");
        e.description = S("Flux");
        e.defaultUnknown = S("Phi");
        e.variables.append(Var("Phi", "flux", "\\Phi", "Wb"));
        e.variables.append(Var("B", "field", "B", "T"));
        e.variables.append(Var("A", "area", "A", "m^2"));
        e.solve.insert(S("Phi"), S("B*A"));
        e.solve.insert(S("B"), S("Phi/A"));
        e.solve.insert(S("A"), S("Phi/B"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("mag_Emf");
        e.field = S("Electromagnetism — Magnetism");
        e.name = S("Motional EMF");
        e.latex = S("\\mathcal{E} = B L v");
        e.description = S("Rod on rails");
        e.defaultUnknown = S("Emf");
        e.variables.append(Var("Emf", "EMF", "\\mathcal{E}", "V"));
        e.variables.append(Var("B", "field", "B", "T"));
        e.variables.append(Var("L", "length", "L", "m"));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.solve.insert(S("Emf"), S("B*L*v"));
        e.solve.insert(S("B"), S("Emf/(L*v)"));
        e.solve.insert(S("L"), S("Emf/(B*v)"));
        e.solve.insert(S("v"), S("Emf/(B*L)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("opt_n");
        e.field = S("Optics");
        e.name = S("Index of refraction");
        e.latex = S("n = \\frac{c}{v}");
        e.description = S("Absolute index");
        e.defaultUnknown = S("n");
        e.variables.append(Var("n", "index", "n", ""));
        e.variables.append(Var("c", "speed of light", "c", "m/s"));
        e.variables.append(Var("v", "speed in medium", "v", "m/s"));
        e.solve.insert(S("n"), S("c/v"));
        e.solve.insert(S("v"), S("c/n"));
        e.solve.insert(S("c"), S("n*v"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("opt_snell");
        e.field = S("Optics");
        e.name = S("Snell's law");
        e.latex = S("n_1 \\sin\\theta_1 = n_2 \\sin\\theta_2");
        e.description = S("Refraction");
        e.defaultUnknown = S("theta2");
        e.variables.append(Var("n1", "index 1", "n_1", ""));
        e.variables.append(Var("theta1", "angle 1", "\\theta_1", "rad"));
        e.variables.append(Var("n2", "index 2", "n_2", ""));
        e.variables.append(Var("theta2", "angle 2", "\\theta_2", "rad"));
        e.solve.insert(S("n2"), S("n1*sin(theta1)/sin(theta2)"));
        e.solve.insert(S("n1"), S("n2*sin(theta2)/sin(theta1)"));
        e.solve.insert(S("theta2"), S("asin(n1*sin(theta1)/n2)"));
        e.solve.insert(S("theta1"), S("asin(n2*sin(theta2)/n1)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("opt_lens");
        e.field = S("Optics");
        e.name = S("Thin-lens equation");
        e.latex = S("\\frac{1}{f} = \\frac{1}{d_o} + \\frac{1}{d_i}");
        e.description = S("Thin lens");
        e.defaultUnknown = S("f");
        e.variables.append(Var("f", "focal length", "f", "m"));
        e.variables.append(Var("do", "object distance", "d_o", "m"));
        e.variables.append(Var("di", "image distance", "d_i", "m"));
        e.solve.insert(S("f"), S("1/(1/do+1/di)"));
        e.solve.insert(S("do"), S("1/(1/f-1/di)"));
        e.solve.insert(S("di"), S("1/(1/f-1/do)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("opt_m");
        e.field = S("Optics");
        e.name = S("Lateral magnification");
        e.latex = S("m = -\\frac{d_i}{d_o}");
        e.description = S("Magnification");
        e.defaultUnknown = S("m");
        e.variables.append(Var("m", "magnification", "m", ""));
        e.variables.append(Var("di", "image distance", "d_i", "m"));
        e.variables.append(Var("do", "object distance", "d_o", "m"));
        e.solve.insert(S("m"), S("-di/do"));
        e.solve.insert(S("di"), S("-m*do"));
        e.solve.insert(S("do"), S("-di/m"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rel_gamma");
        e.field = S("Modern Physics — Relativity");
        e.name = S("Lorentz factor");
        e.latex = S("\\gamma = \\frac{1}{\\sqrt{1-v^2/c^2}}");
        e.description = S("Gamma");
        e.defaultUnknown = S("gamma");
        e.variables.append(Var("gamma", "Lorentz factor", "\\gamma", ""));
        e.variables.append(Var("v", "speed", "v", "m/s"));
        e.variables.append(Var("c", "speed of light", "c", "m/s"));
        e.solve.insert(S("gamma"), S("1/sqrt(1-v^2/c^2)"));
        e.solve.insert(S("v"), S("c*sqrt(1-1/gamma^2)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rel_E0");
        e.field = S("Modern Physics — Relativity");
        e.name = S("Rest energy");
        e.latex = S("E_0 = m c^2");
        e.description = S("Rest energy");
        e.defaultUnknown = S("E0");
        e.variables.append(Var("E0", "rest energy", "E_0", "J"));
        e.variables.append(Var("m", "mass", "m", "kg"));
        e.variables.append(Var("c", "speed of light", "c", "m/s"));
        e.solve.insert(S("E0"), S("m*c^2"));
        e.solve.insert(S("m"), S("E0/c^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rel_E");
        e.field = S("Modern Physics — Relativity");
        e.name = S("Total relativistic energy");
        e.latex = S("E = \\gamma m c^2");
        e.description = S("Total energy");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "energy", "E", "J"));
        e.variables.append(Var("gamma", "Lorentz factor", "\\gamma", ""));
        e.variables.append(Var("m", "rest mass", "m", "kg"));
        e.variables.append(Var("c", "c", "c", "m/s"));
        e.solve.insert(S("E"), S("gamma*m*c^2"));
        e.solve.insert(S("gamma"), S("E/(m*c^2)"));
        e.solve.insert(S("m"), S("E/(gamma*c^2)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rel_t");
        e.field = S("Modern Physics — Relativity");
        e.name = S("Time dilation");
        e.latex = S("\\Delta t = \\gamma \\Delta t_0");
        e.description = S("Moving clocks");
        e.defaultUnknown = S("dt");
        e.variables.append(Var("dt", "lab time", "\\Delta t", "s"));
        e.variables.append(Var("gamma", "γ", "\\gamma", ""));
        e.variables.append(Var("dt0", "proper time", "\\Delta t_0", "s"));
        e.solve.insert(S("dt"), S("gamma*dt0"));
        e.solve.insert(S("dt0"), S("dt/gamma"));
        e.solve.insert(S("gamma"), S("dt/dt0"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("rel_L");
        e.field = S("Modern Physics — Relativity");
        e.name = S("Length contraction");
        e.latex = S("L = \\frac{L_0}{\\gamma}");
        e.description = S("Length in motion");
        e.defaultUnknown = S("L");
        e.variables.append(Var("L", "contracted length", "L", "m"));
        e.variables.append(Var("L0", "proper length", "L_0", "m"));
        e.variables.append(Var("gamma", "γ", "\\gamma", ""));
        e.solve.insert(S("L"), S("L0/gamma"));
        e.solve.insert(S("L0"), S("L*gamma"));
        e.solve.insert(S("gamma"), S("L0/L"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("qu_E");
        e.field = S("Modern Physics — Quantum");
        e.name = S("Photon energy");
        e.latex = S("E = h f");
        e.description = S("Planck relation");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "energy", "E", "J"));
        e.variables.append(Var("h", "Planck", "h", "J·s"));
        e.variables.append(Var("f", "frequency", "f", "Hz"));
        e.solve.insert(S("E"), S("h*f"));
        e.solve.insert(S("f"), S("E/h"));
        e.solve.insert(S("h"), S("E/f"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("qu_p");
        e.field = S("Modern Physics — Quantum");
        e.name = S("de Broglie wavelength");
        e.latex = S("\\lambda = \\frac{h}{p}");
        e.description = S("Matter wave");
        e.defaultUnknown = S("lambda");
        e.variables.append(Var("lambda", "wavelength", "\\lambda", "m"));
        e.variables.append(Var("h", "Planck", "h", "J·s"));
        e.variables.append(Var("p", "momentum", "p", "kg·m/s"));
        e.solve.insert(S("lambda"), S("h/p"));
        e.solve.insert(S("p"), S("h/lambda"));
        e.solve.insert(S("h"), S("lambda*p"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("qu_photo");
        e.field = S("Modern Physics — Quantum");
        e.name = S("Photoelectric equation");
        e.latex = S("K_{max} = h f - W");
        e.description = S("Einstein photoelectric");
        e.defaultUnknown = S("Kmax");
        e.variables.append(Var("Kmax", "max KE", "K_{max}", "J"));
        e.variables.append(Var("h", "Planck", "h", "J·s"));
        e.variables.append(Var("f", "frequency", "f", "Hz"));
        e.variables.append(Var("W", "work function", "W", "J"));
        e.solve.insert(S("Kmax"), S("h*f-W"));
        e.solve.insert(S("W"), S("h*f-Kmax"));
        e.solve.insert(S("f"), S("(Kmax+W)/h"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("qu_hbar");
        e.field = S("Modern Physics — Quantum");
        e.name = S("E = ħω");
        e.latex = S("E = \\hbar \\omega");
        e.description = S("Angular form");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "energy", "E", "J"));
        e.variables.append(Var("hbar", "ħ", "\\hbar", "J·s"));
        e.variables.append(Var("omega", "angular frequency", "\\omega", "rad/s"));
        e.solve.insert(S("E"), S("hbar*omega"));
        e.solve.insert(S("omega"), S("E/hbar"));
        e.solve.insert(S("hbar"), S("E/omega"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("nuc_half");
        e.field = S("Modern Physics — Nuclear");
        e.name = S("Half-life");
        e.latex = S("T_{1/2} = \\frac{\\ln 2}{\\lambda}");
        e.description = S("Radioactive half-life");
        e.defaultUnknown = S("T12");
        e.variables.append(Var("T12", "half-life", "T_{1/2}", "s"));
        e.variables.append(Var("lambda", "decay constant", "\\lambda", "1/s"));
        e.solve.insert(S("T12"), S("ln(2)/lambda"));
        e.solve.insert(S("lambda"), S("ln(2)/T12"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("nuc_decay");
        e.field = S("Modern Physics — Nuclear");
        e.name = S("Exponential decay (activity)");
        e.latex = S("A = A_0 e^{-\\lambda t}");
        e.description = S("Activity vs time");
        e.defaultUnknown = S("A");
        e.variables.append(Var("A", "activity", "A", "Bq"));
        e.variables.append(Var("A0", "initial activity", "A_0", "Bq"));
        e.variables.append(Var("lambda", "decay constant", "\\lambda", "1/s"));
        e.variables.append(Var("t", "time", "t", "s"));
        e.solve.insert(S("A"), S("A0*exp(-lambda*t)"));
        e.solve.insert(S("A0"), S("A/exp(-lambda*t)"));
        e.solve.insert(S("t"), S("-ln(A/A0)/lambda"));
        e.solve.insert(S("lambda"), S("-ln(A/A0)/t"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("nuc_E");
        e.field = S("Modern Physics — Nuclear");
        e.name = S("Mass–energy for defect");
        e.latex = S("E = \\Delta m\\, c^2");
        e.description = S("Energy from mass defect");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "energy", "E", "J"));
        e.variables.append(Var("dm", "mass defect", "\\Delta m", "kg"));
        e.variables.append(Var("c", "c", "c", "m/s"));
        e.solve.insert(S("E"), S("dm*c^2"));
        e.solve.insert(S("dm"), S("E/c^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("kin_freefall");
        e.field = S("Mechanics — Kinematics");
        e.name = S("Free-fall distance");
        e.latex = S("h = \\frac{1}{2} g t^2");
        e.description = S("From rest");
        e.defaultUnknown = S("h");
        e.variables.append(Var("h", "height", "h", "m"));
        e.variables.append(Var("g", "gravity", "g", "m/s^2"));
        e.variables.append(Var("t", "time", "t", "s"));
        e.solve.insert(S("h"), S("0.5*g*t^2"));
        e.solve.insert(S("t"), S("sqrt(2*h/g)"));
        e.solve.insert(S("g"), S("2*h/t^2"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("en_hooke");
        e.field = S("Mechanics — Dynamics");
        e.name = S("Hooke's law");
        e.latex = S("F = -k x");
        e.description = S("Restoring force (magnitude k|x| for solve use F=k*x)");
        e.defaultUnknown = S("F");
        e.variables.append(Var("F", "force magnitude", "F", "N"));
        e.variables.append(Var("k", "spring constant", "k", "N/m"));
        e.variables.append(Var("x", "extension", "x", "m"));
        e.solve.insert(S("F"), S("k*x"));
        e.solve.insert(S("k"), S("F/x"));
        e.solve.insert(S("x"), S("F/k"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("wave_beat");
        e.field = S("Waves & Oscillations");
        e.name = S("Beat frequency");
        e.latex = S("f_{beat} = |f_1 - f_2|");
        e.description = S("Beats");
        e.defaultUnknown = S("fbeat");
        e.variables.append(Var("fbeat", "beat frequency", "f_{beat}", "Hz"));
        e.variables.append(Var("f1", "frequency 1", "f_1", "Hz"));
        e.variables.append(Var("f2", "frequency 2", "f_2", "Hz"));
        e.solve.insert(S("fbeat"), S("abs(f1-f2)"));
        e.solve.insert(S("f1"), S("f2+fbeat"));
        e.solve.insert(S("f2"), S("f1-fbeat"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("th_eff");
        e.field = S("Thermodynamics");
        e.name = S("Heat-engine efficiency");
        e.latex = S("\\eta = \\frac{W}{Q_h}");
        e.description = S("Efficiency");
        e.defaultUnknown = S("eta");
        e.variables.append(Var("eta", "efficiency", "\\eta", ""));
        e.variables.append(Var("W", "work output", "W", "J"));
        e.variables.append(Var("Qh", "heat input", "Q_h", "J"));
        e.solve.insert(S("eta"), S("W/Qh"));
        e.solve.insert(S("W"), S("eta*Qh"));
        e.solve.insert(S("Qh"), S("W/eta"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("es_U");
        e.field = S("Electromagnetism — Electrostatics");
        e.name = S("Potential energy of two charges");
        e.latex = S("U = k_e \\frac{q_1 q_2}{r}");
        e.description = S("Electrostatic PE");
        e.defaultUnknown = S("U");
        e.variables.append(Var("U", "potential energy", "U", "J"));
        e.variables.append(Var("q1", "q1", "q_1", "C"));
        e.variables.append(Var("q2", "q2", "q_2", "C"));
        e.variables.append(Var("r", "r", "r", "m"));
        e.variables.append(Var("ke", "k_e", "k_e", "N·m^2/C^2"));
        e.solve.insert(S("U"), S("ke*q1*q2/r"));
        e.solve.insert(S("r"), S("ke*q1*q2/U"));
        e.solve.insert(S("q1"), S("U*r/(ke*q2)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("cir_Ps");
        e.field = S("Electromagnetism — Circuits");
        e.name = S("Power P=V²/R");
        e.latex = S("P = \\frac{V^2}{R}");
        e.description = S("Alternative power form");
        e.defaultUnknown = S("P");
        e.variables.append(Var("P", "power", "P", "W"));
        e.variables.append(Var("V", "voltage", "V", "V"));
        e.variables.append(Var("R", "resistance", "R", "Ω"));
        e.solve.insert(S("P"), S("V^2/R"));
        e.solve.insert(S("R"), S("V^2/P"));
        e.solve.insert(S("V"), S("sqrt(P*R)"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("opt_mirror");
        e.field = S("Optics");
        e.name = S("Mirror / lens magnification from heights");
        e.latex = S("m = \\frac{h_i}{h_o}");
        e.description = S("Height ratio");
        e.defaultUnknown = S("m");
        e.variables.append(Var("m", "magnification", "m", ""));
        e.variables.append(Var("hi", "image height", "h_i", "m"));
        e.variables.append(Var("ho", "object height", "h_o", "m"));
        e.solve.insert(S("m"), S("hi/ho"));
        e.solve.insert(S("hi"), S("m*ho"));
        e.solve.insert(S("ho"), S("hi/m"));
        add(std::move(e));
    }
    {
        PhysicsEquation e;
        e.id = S("qu_Elambda");
        e.field = S("Modern Physics — Quantum");
        e.name = S("Photon energy from wavelength");
        e.latex = S("E = \\frac{h c}{\\lambda}");
        e.description = S("E–λ relation");
        e.defaultUnknown = S("E");
        e.variables.append(Var("E", "energy", "E", "J"));
        e.variables.append(Var("h", "Planck", "h", "J·s"));
        e.variables.append(Var("c", "c", "c", "m/s"));
        e.variables.append(Var("lambda", "wavelength", "\\lambda", "m"));
        e.solve.insert(S("E"), S("h*c/lambda"));
        e.solve.insert(S("lambda"), S("h*c/E"));
        add(std::move(e));
    }
    return cache;
}

QStringList PhysicsCatalog::fields()
{
    QSet<QString> set;
    for (const auto &e : equations()) set.insert(e.field);
    QStringList list = set.values();
    std::sort(list.begin(), list.end());
    return list;
}

QVector<PhysicsEquation> PhysicsCatalog::equationsInField(const QString &field)
{
    QVector<PhysicsEquation> out;
    for (const auto &e : equations())
        if (e.field == field) out.append(e);
    return out;
}

const PhysicsEquation *PhysicsCatalog::equationById(const QString &id)
{
    const auto &all = equations();
    for (const auto &e : all) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}
