// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.h"

// ==========================================================================
// Custom Combo Point Impl.
// ==========================================================================

#define COMBO_POINTS_MAX 5

static inline int clamp( int x, int low, int high )
{ 
  return x < low ? low : (x > high ? high : x);
}

struct combo_points_t
{
  sim_t* sim;
  player_t* player;
  
  proc_t* proc;
  proc_t* wasted;
  
  int count;

  combo_points_t( player_t* p ) :
    sim( p -> sim ), player( p ), count( 0 )
  {
    proc   = player -> get_proc( "combo_points" );
    wasted = player -> get_proc( "combo_points_wasted" );
  }

  void add( int num, const char* action = 0 )
  {    
    int actual_num = clamp( num, 0, COMBO_POINTS_MAX - count );
    int overflow   = num - actual_num;

    // we count all combo points gained in the proc
    for ( int i = 0; i < num; i++ )
      proc -> occur();
    
    // add actual combo points
    if ( actual_num > 0 )
      count += actual_num;
    
    // count wasted combo points
    if ( overflow > 0 )
    {
      for ( int i = 0; i < overflow; i++ )
        wasted -> occur();
    }

    if ( sim -> log )
    {
      if ( action )
        log_t::output( sim, "%s gains %d (%d) combo_points from %s (%d)", 
                       player -> name(), actual_num, num, action, count );
      else
        log_t::output( sim, "%s gains %d (%d) combo_points (%d)", 
                       player -> name(), actual_num, num, count );
    }
  }

  void add( int num, const spell_id_t* spell )
  {
    add( num, spell -> token().c_str() );
  }
  
  void clear( const char* action = 0 )
  {
    if ( sim -> log )
    {
      if (action )
        log_t::output( sim, "%s spends %d combo_points on %s", 
                       player -> name(), count, action);
      else
        log_t::output( sim, "%s loses %d combo_points", 
                       player -> name(), count);
    }

    count = 0;
  }

  double rank( double* cp_list ) SC_CONST
  {
    assert( count > 0 );
    return cp_list[ count - 1 ];
  }
  double rank( double cp1, double cp2, double cp3, double cp4, double cp5 ) SC_CONST
  {
    double cp_list[] = { cp1, cp2, cp3, cp4, cp5 };
    return rank( cp_list );
  }
};

// ==========================================================================
// Rogue
// ==========================================================================

// ==========================================================================
// Todo for Cata :
//  New Ability: Redirect
//  New Glyph: Hemorrhage
//  Review: Ability Scaling
//  Review: Bandit's Guile
//  Review: Energy Regen (how Adrenaline rush stacks with Blade Flurry / haste)
// ==========================================================================

enum poison_type_t { POISON_NONE=0, ANESTHETIC_POISON, DEADLY_POISON, INSTANT_POISON, WOUND_POISON };

struct rogue_t : public player_t
{
  // Active
  action_t* active_deadly_poison;
  action_t* active_instant_poison;
  action_t* active_wound_poison;
  action_t* active_venomous_wound;
  action_t* active_main_gauche;

  // Buffs
  buff_t* buffs_bandits_guile;
  buff_t* buffs_deadly_proc;
  buff_t* buffs_overkill;
  buff_t* buffs_poison_doses;
  buff_t* buffs_recuperate;
  buff_t* buffs_shiv;
  buff_t* buffs_stealthed;
  buff_t* buffs_tier11_4pc;
  buff_t* buffs_vanish;
  
  new_buff_t* buffs_adrenaline_rush;
  new_buff_t* buffs_blade_flurry;
  new_buff_t* buffs_cold_blood;
  new_buff_t* buffs_envenom;
  new_buff_t* buffs_find_weakness;
  new_buff_t* buffs_killing_spree;
  new_buff_t* buffs_master_of_subtlety;
  new_buff_t* buffs_revealing_strike;
  new_buff_t* buffs_shadow_dance;
  new_buff_t* buffs_shadowstep;
  new_buff_t* buffs_slice_and_dice;
  new_buff_t* buffs_vendetta;

  // Cooldowns
  cooldown_t* cooldowns_honor_among_thieves;
  cooldown_t* cooldowns_seal_fate;
  cooldown_t* cooldowns_adrenaline_rush;
  cooldown_t* cooldowns_killing_spree;

  // Dots
  dot_t* dots_rupture;

  // Expirations
  struct _expirations_t
  {
    event_t* wound_poison;

    void reset() { memset( ( void* ) this, 0x00, sizeof( _expirations_t ) ); }
    _expirations_t() { reset(); }
  };
  _expirations_t _expirations;

  // Gains
  gain_t* gains_adrenaline_rush;
  gain_t* gains_backstab_glyph;
  gain_t* gains_cold_blood;
  gain_t* gains_combat_potency;
  gain_t* gains_energy_refund;
  gain_t* gains_murderous_intent;
  gain_t* gains_overkill;
  gain_t* gains_recuperate;
  gain_t* gains_relentless_strikes;
  gain_t* gains_venomous_vim;
  gain_t* gains_tier10_2pc;

  // Procs
  proc_t* procs_deadly_poison;
  proc_t* procs_honor_among_thieves;
  proc_t* procs_ruthlessness;
  proc_t* procs_seal_fate;
  proc_t* procs_serrated_blades;
  proc_t* procs_sinister_strike_glyph;
  proc_t* procs_main_gauche;
  proc_t* procs_venomous_wounds;
  proc_t* procs_tier10_4pc;

  // Up-Times
  uptime_t* uptimes_bandits_guile[ 3 ];
  uptime_t* uptimes_energy_cap;
  uptime_t* uptimes_poisoned;
  uptime_t* uptimes_rupture;

  // Random Number Generation
  rng_t* rng_anesthetic_poison;
  rng_t* rng_bandits_guile;
  rng_t* rng_combat_potency;
  rng_t* rng_critical_strike_interval;
  rng_t* rng_cut_to_the_chase;
  rng_t* rng_deadly_poison;
  rng_t* rng_honor_among_thieves;
  rng_t* rng_improved_expose_armor;
  rng_t* rng_initiative;
  rng_t* rng_instant_poison;
  rng_t* rng_main_gauche;
  rng_t* rng_relentless_strikes;
  rng_t* rng_ruthlessness;
  rng_t* rng_seal_fate;
  rng_t* rng_serrated_blades;
  rng_t* rng_sinister_strike_glyph;
  rng_t* rng_venomous_wounds;
  rng_t* rng_wound_poison;
  rng_t* rng_tier10_4pc;

  // Spec passives
  passive_spell_t* spec_improved_poisons;   // done
  passive_spell_t* spec_assassins_resolve;  // done
  passive_spell_t* spec_vitality;           // done
  passive_spell_t* spec_ambidexterity;      // done
  passive_spell_t* spec_master_of_subtlety; // done
  passive_spell_t* spec_sinister_calling;   // done

  // Spec actives
  active_spell_t* spec_mutilate;
  active_spell_t* spec_blade_flurry;
  active_spell_t* spec_shadowstep;

  // Masteries
  mastery_t* mastery_potent_poisons; // XXX done as additive
  mastery_t* mastery_main_gauche;    // done
  mastery_t* mastery_executioner;    // XXX done as additive

  // Talents
  struct talents_list_t
  {
    // Assasination
    talent_t* cold_blood;               // done
    talent_t* coup_de_grace;            // done
    talent_t* cut_to_the_chase;         // done
    talent_t* improved_expose_armor;    // XXX: maybe add proc counting for 1/2 setups
    talent_t* lethality;                // done
    talent_t* master_poisoner;          // done
    talent_t* murderous_intent;         // done
    talent_t* overkill;                 // done
    talent_t* puncturing_wounds;        // done
    talent_t* quickening;               // XXX: there is no run speed in simc
    talent_t* ruthlessness;             // done
    talent_t* seal_fate;                // done
    talent_t* vendetta;                 // done
    talent_t* venomous_wounds;          // done: energy return on target death is not implemented (as we only ever simulate 1 target till it dies)
    talent_t* vile_poisons;             // done

    // Combat
    talent_t* adrenaline_rush;          // done
    talent_t* aggression;               // done
    talent_t* bandits_guile;            // done XXX incomplete: it updates damage 'in real time' (so rupture ticks for example get boosted as you build it up)
    talent_t* combat_potency;           // done
    talent_t* improved_sinister_strike; // done
    talent_t* improved_slice_and_dice;  // done
    talent_t* killing_spree;            // done
    talent_t* lightning_reflexes;       // done
    talent_t* precision;                // done
    talent_t* restless_blades;          // done
    talent_t* revealing_strike;         // XXX: does it get eaten by missed finishers?
    talent_t* savage_combat;            // done

    // Subtlety
    talent_t* elusiveness;                // done
    talent_t* energetic_recovery;         // done
    talent_t* find_weakness;              // done
    talent_t* hemorrhage;                 // XXX raid aura / target debuff state
    talent_t* honor_among_thieves;        // XXX more changes needed?
    talent_t* improved_ambush;            // done
    talent_t* initiative;                 // done
    talent_t* opportunity;                // done
    talent_t* premeditation;              // done
    talent_t* preparation;                // done
    talent_t* relentless_strikes;         // done
    talent_t* sanguinary_vein;            // done
    talent_t* serrated_blades;            // XXX: review to check if rupture refreshes the "right" way
    talent_t* shadow_dance;               // done
    talent_t* slaughter_from_the_shadows; // done
    talent_t* waylay;                     // XXX
  };
  talents_list_t talents;

  struct glyphs_t
  {
    glyph_t* adrenaline_rush;     // done
    glyph_t* ambush;              // XXX
    glyph_t* backstab;            // done
    glyph_t* blade_flurry;        // done
    glyph_t* eviscerate;          // done
    glyph_t* expose_armor;        // done
    glyph_t* feint;               // done
    glyph_t* hemorrhage;          // XXX
    glyph_t* kick;                // XXX
    glyph_t* killing_spree;       // done
    glyph_t* mutilate;            // done
    glyph_t* preparation;         // done
    glyph_t* revealing_strike;    // done
    glyph_t* rupture;             // done
    glyph_t* shadow_dance;        // done
    glyph_t* sinister_strike;     // done
    glyph_t* slice_and_dice;      // done
    glyph_t* tricks_of_the_trade; // done
    glyph_t* vendetta;            // done
  };
  glyphs_t glyphs;

  combo_points_t* combo_points;

  // Options
  std::vector<action_callback_t*> critical_strike_callbacks;
  std::vector<double> critical_strike_intervals;
  std::string critical_strike_intervals_str;
  std::string tricks_of_the_trade_target_str;
  player_t*   tricks_of_the_trade_target;

  rogue_t( sim_t* sim, const std::string& name, race_type r = RACE_NONE ) : player_t( sim, ROGUE, name, r )
  {
    tree_type[ ROGUE_ASSASSINATION ] = TREE_ASSASSINATION;
    tree_type[ ROGUE_COMBAT        ] = TREE_COMBAT;
    tree_type[ ROGUE_SUBTLETY      ] = TREE_SUBTLETY;

    // Active
    active_deadly_poison  = 0;
    active_instant_poison = 0;
    active_wound_poison   = 0;
    active_venomous_wound = 0;
    active_main_gauche    = 0;

    combo_points          = new combo_points_t( this );

    // Dots
    dots_rupture          = get_dot( "rupture" );

    // Cooldowns
    cooldowns_honor_among_thieves = get_cooldown( "honor_among_thieves" );
    cooldowns_seal_fate           = get_cooldown( "seal_fate"           );
    cooldowns_adrenaline_rush     = get_cooldown( "adrenaline_rush"     );
    cooldowns_killing_spree       = get_cooldown( "killing_spree"       );

    // Options
    critical_strike_intervals_str = "1.50/1.75/2.0/2.25";
    tricks_of_the_trade_target_str = "other";
    tricks_of_the_trade_target = 0;
  }

  ~rogue_t()
  {
    if ( combo_points )
      delete combo_points;
  }
  
  // Character Definition
  virtual void      init_talents();
  virtual void      init_spells();
  virtual void      init_glyphs();
  virtual void      init_race();
  virtual void      init_base();
  virtual void      init_gains();
  virtual void      init_procs();
  virtual void      init_uptimes();
  virtual void      init_rng();
  virtual void      init_scaling();
  virtual void      init_buffs();
  virtual void      init_actions();
  virtual void      init_values();
  virtual void      register_callbacks();
  virtual void      combat_begin();
  virtual void      reset();
  virtual void      interrupt();
  virtual void      clear_debuffs();
  virtual void      regen( double periodicity );
  virtual double    available() SC_CONST;
  virtual std::vector<talent_translation_t>& get_talent_list();
  virtual std::vector<option_t>& get_options();
  virtual action_t* create_action( const std::string& name, const std::string& options );
  virtual action_expr_t* create_expression( action_t* a, const std::string& name_str );
  virtual int       decode_set( item_t& item );
  virtual int       primary_resource() SC_CONST { return RESOURCE_ENERGY; }
  virtual int       primary_role() SC_CONST     { return ROLE_ATTACK; }
  virtual bool      create_profile( std::string& profile_str, int save_type=SAVE_ALL );

  virtual double    composite_attribute_multiplier( int attr ) SC_CONST;
  virtual double    matching_gear_multiplier( const attribute_type attr ) SC_CONST;
  virtual double    composite_attack_power_multiplier() SC_CONST;
  virtual double    composite_player_multiplier( const school_type school ) SC_CONST;
};

namespace // ANONYMOUS NAMESPACE ============================================
{

// ==========================================================================
// Rogue Attack
// ==========================================================================

struct rogue_attack_t : public attack_t
{
  /**
   * Simple actions do not invoke attack_t ::execute and ::player_buff methods.
   * It's added for simple buff actions ( like AR, CB etc. ) which may consume resources,
   * require CPs etc, but don't do any damage (by themselves at least).
   */
  bool simple;
  buff_t* m_buff;

  int  requires_weapon;
  int  requires_position;
  bool requires_stealth;
  bool requires_combo_points;

  // we now track how much CPs can an action give us
  int adds_combo_points;

  // we now track how much combo points we spent on an action
  int combo_points_spent;
  
  rogue_attack_t( const char* n, rogue_t* p, bool special = false ) :
    attack_t( n, p, RESOURCE_ENERGY, SCHOOL_PHYSICAL, TREE_NONE, special ), 
    simple( false ), m_buff( 0 ), adds_combo_points( 0 )
  {
    _init_rogue_attack_t();
  }
  
  rogue_attack_t( const char* n, uint32_t id, rogue_t* p, bool simple = false ) :
    attack_t( n, id, p, 0, true ), 
    simple( simple ), m_buff( 0 ), adds_combo_points( 0 )
  {
    _init_rogue_attack_t();
  }
    
  void _init_rogue_attack_t();
  
  virtual void   parse_options( option_t*, const std::string& options_str );
  virtual void   parse_options( const std::string& options_str );
  virtual double cost() SC_CONST;
  virtual void   consume_resource();
  virtual void   execute();
  virtual double calculate_weapon_damage();
  virtual void   player_buff();
  virtual bool   ready();
  virtual void   assess_damage( double amount, int dmg_type );
  virtual double total_multiplier() SC_CONST;
  virtual double armor() SC_CONST;

  void add_combo_points();
  void add_trigger_buff( buff_t* buff );
  void trigger_buff();
};

// ==========================================================================
// Rogue Poison
// ==========================================================================

struct rogue_poison_t : public spell_t
{
  rogue_poison_t( const char* n, const uint32_t id, rogue_t* p ) :
    spell_t( n, id, p )
  {
    proc             = true;
    background       = true;
    base_cost        = 0;
    trigger_gcd      = 0;
    may_crit         = true;
    tick_may_crit    = true;
    hasted_ticks     = false;
    
    base_hit         += p -> talents.precision -> base_value( E_APPLY_AURA, A_MOD_SPELL_HIT_CHANCE );
    base_multiplier  *= 1.0 + p -> talents.vile_poisons -> base_value( E_APPLY_AURA, A_ADD_PCT_MODIFIER );
    weapon_multiplier = 0;

    // Poisons are spells that use attack power
    base_spell_power_multiplier  = 0.0;
    base_attack_power_multiplier = 1.0;
  }
  
  virtual double total_multiplier() SC_CONST;
};

// ==========================================================================
// Static Functions
// ==========================================================================

// break_stealth ===========================================================

static void break_stealth( rogue_t* p )
{
  if ( p -> buffs_stealthed -> check() )
  {
    p -> buffs_stealthed -> expire();

    if ( p -> spec_master_of_subtlety -> ok() )
      p -> buffs_master_of_subtlety -> trigger();

    if ( p -> talents.overkill -> rank() )
      p -> buffs_overkill -> trigger();
  }

  if ( p -> buffs_vanish -> check() )
  {
    p -> buffs_vanish -> expire();

    if ( p -> talents.overkill -> rank() )
      p -> buffs_overkill -> trigger();
  }
}

// trigger_apply_poisons ===================================================

static void trigger_apply_poisons( rogue_attack_t* a )
{
  weapon_t* w = a -> weapon;

  if ( ! w ) 
    return;

  rogue_t* p = a -> player -> cast_rogue();

  if ( w -> buff_type == DEADLY_POISON )
  {
    p -> active_deadly_poison -> weapon = w;
    p -> active_deadly_poison -> execute();
  }
  else if ( w -> buff_type == INSTANT_POISON )
  {
    p -> active_instant_poison -> weapon = w;
    p -> active_instant_poison -> execute();
  }
  else if ( w -> buff_type == WOUND_POISON )
  {
    p -> active_wound_poison -> weapon = w;
    p -> active_wound_poison -> execute();
  }
}

// trigger_bandits_guile ===================================================

static void trigger_bandits_guile( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.bandits_guile -> rank() )
    return;

  int current_stack = p -> buffs_bandits_guile -> current_stack;
  if ( current_stack == p -> buffs_bandits_guile -> max_stack )
    return; // we can't refresh the 15% buff

  if ( p -> rng_bandits_guile -> roll( p -> talents.bandits_guile -> rank() / 3.0 ) )
    p -> buffs_bandits_guile -> trigger( 1, ( (current_stack + 1) / 4 ) * 0.10 );
}

// trigger_other_poisons ===================================================

static void trigger_other_poisons( rogue_t* p, weapon_t* other_w )
{
  if ( ! other_w ) 
    return;

  if ( other_w -> buff_type == INSTANT_POISON )
  {
    p -> active_instant_poison -> weapon = other_w;
    p -> active_instant_poison -> execute();
  }
  else if ( other_w -> buff_type == WOUND_POISON )
  {
    p -> active_wound_poison -> weapon = other_w;
    p -> active_wound_poison -> execute();
  }
}

// trigger_combat_potency ==================================================

static void trigger_combat_potency( rogue_attack_t* a )
{
  weapon_t* w = a -> weapon;

  if ( ! w || w -> slot != SLOT_OFF_HAND )
    return;

  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.combat_potency -> rank() )
    return;

  if ( p -> rng_combat_potency -> roll( p -> talents.combat_potency -> proc_chance() ) )
  {
    // energy gain value is in the proc trigger spell
    p -> resource_gain( RESOURCE_ENERGY, p -> talents.combat_potency -> rank() * 5.0, p -> gains_combat_potency );
  }
}

// trigger_cut_to_the_chase ================================================

static void trigger_cut_to_the_chase( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.cut_to_the_chase -> rank() )
    return;

  if ( ! p -> buffs_slice_and_dice -> check() )
    return;

  if ( p -> rng_cut_to_the_chase -> roll( p -> talents.cut_to_the_chase -> proc_chance() ) )
    p -> buffs_slice_and_dice -> trigger( COMBO_POINTS_MAX );
}

// trigger_initiative ======================================================

static void trigger_initiative( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.initiative -> rank() )
    return;

  if ( p -> rng_initiative -> roll( p -> talents.initiative -> base_value( E_APPLY_AURA, A_ADD_TARGET_TRIGGER ) / 100.0 ) )
    p -> combo_points -> add( 1, p -> talents.initiative );
}

// trigger_energy_refund ===================================================

static void trigger_energy_refund( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! a -> adds_combo_points )
    return;

  double energy_restored = a -> resource_consumed * 0.80;

  p -> resource_gain( RESOURCE_ENERGY, energy_restored, p -> gains_energy_refund );
}

// trigger_find_weakness ====================================================

static void trigger_find_weakness( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.find_weakness -> rank() )
    return;

  p -> buffs_find_weakness -> trigger();
};

// trigger_relentless_strikes ===============================================

static void trigger_relentless_strikes( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.relentless_strikes -> rank() )
    return;

  if ( ! a -> requires_combo_points )
    return;

  double chance = p -> talents.relentless_strikes -> effect_pp_combo_points( 1 ) / 100.0;
  if ( p -> rng_relentless_strikes -> roll( chance * p -> combo_points -> count ) )
  {
    // actual energy gain is in Relentless Strike Effect (14181)
    double gain = p -> player_data.effect_base_value( 14181, E_ENERGIZE );
    p -> resource_gain( RESOURCE_ENERGY, gain, p -> gains_relentless_strikes );
  }
}

// trigger_restless_blades ==================================================

static void trigger_restless_blades( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.restless_blades -> rank() )
    return;

  if ( ! a -> requires_combo_points )
    return;

  double reduction = ( p -> talents.restless_blades -> base_value() / 1000.0 ) * a -> combo_points_spent;

  p -> cooldowns_adrenaline_rush -> ready -= reduction;
  p -> cooldowns_killing_spree -> ready   -= reduction;
}

// trigger_ruthlessness ====================================================

static void trigger_ruthlessness( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.ruthlessness -> rank() )
    return;

  if ( ! a -> requires_combo_points )
    return;

  if ( p -> rng_ruthlessness -> roll( p -> talents.ruthlessness -> proc_chance() ) )
  {
    p -> procs_ruthlessness -> occur();
    p -> combo_points -> add( 1, p -> talents.ruthlessness );
  }
}

// trigger_seal_fate =======================================================

static void trigger_seal_fate( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.seal_fate -> rank() )
    return;

  if ( ! a -> adds_combo_points )
    return;

  // This is to prevent dual-weapon special attacks from triggering a double-proc of Seal Fate
  if ( p -> cooldowns_seal_fate -> remains() > 0 )
    return;

  if ( p -> rng_seal_fate -> roll( p -> talents.seal_fate -> proc_chance() ) )
  {
    p -> cooldowns_seal_fate -> start( 0.0001 );
    p -> procs_seal_fate -> occur();
    p -> combo_points -> add( 1, p -> talents.seal_fate );
  }
}

// trigger_tier10_4pc =======================================================

static void trigger_tier10_4pc( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> set_bonus.tier10_4pc_melee() )
    return;

  if ( ! a -> requires_combo_points )
    return;

  if ( p -> rng_tier10_4pc -> roll( 0.13 ) )
  {
    p -> procs_tier10_4pc -> occur();
    p -> combo_points -> add( 3, "tier10_4pc" );
  }
}

// trigger_main_gauche ======================================================

static void trigger_main_gauche( rogue_attack_t* a )
{
  if ( a -> proc )
    return;
  
  weapon_t* w = a -> weapon;

  if ( ! w || w -> slot != SLOT_MAIN_HAND )
    return;

  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> mastery_main_gauche -> ok() )
    return;

  double chance = p -> composite_mastery() * p -> mastery_main_gauche -> base_value( E_APPLY_AURA, A_DUMMY, 0 );

  if ( p -> rng_main_gauche -> roll( chance ) )
  {
    if ( ! p -> active_main_gauche )
    {
      struct main_gouche_t : public rogue_attack_t
      {
        main_gouche_t( rogue_t* p ) : rogue_attack_t( "main_gauche", p )
        {
          weapon = &( p -> off_hand_weapon );

          base_dd_min = base_dd_max = 1;
          background      = true;
          trigger_gcd     = 0;
          base_cost       = 0;
          may_glance      = false; // XXX: does not glance
          may_crit        = true;
          normalize_weapon_speed = true; // XXX: it's normalized

          reset();
        }

        virtual void execute()
        {
          rogue_attack_t::execute();

          if ( result_is_hit() )
            trigger_combat_potency( this );
        }
      };
      p -> active_main_gauche = new main_gouche_t( p );
    }
    
    p -> active_main_gauche -> execute();
    p -> procs_main_gauche  -> occur();
  }
}

// trigger_tricks_of_the_trade ==============================================

static void trigger_tricks_of_the_trade( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  player_t* t = p -> tricks_of_the_trade_target;

  if ( t )
  {
    double duration = p -> player_data.spell_duration( 57933 );

    if ( t -> buffs.tricks_of_the_trade -> remains_lt( duration ) )
    {
      double value = p -> player_data.effect_base_value( 57933, E_APPLY_AURA );
      value += p -> glyphs.tricks_of_the_trade -> mod_additive( P_EFFECT_1 );
      
      t -> buffs.tricks_of_the_trade -> buff_duration = duration;
      t -> buffs.tricks_of_the_trade -> trigger( 1, value / 100.0 );
    }

    p -> tricks_of_the_trade_target = 0;
  }
}

// trigger_venomous_wounds ==================================================

static void trigger_venomous_wounds( rogue_attack_t* a )
{
  rogue_t* p = a -> player -> cast_rogue();

  if ( ! p -> talents.venomous_wounds -> rank() )
    return;

  // we currently only check for presence of deadly poison
  if ( ! p -> buffs_poison_doses -> check() )
    return;

  if ( p -> rng_venomous_wounds -> roll( p -> talents.venomous_wounds -> proc_chance() ) )
  {    
    if ( ! p -> active_venomous_wound )
    {
      struct venomous_wound_t : public rogue_poison_t
      {
        venomous_wound_t( rogue_t* p ) : rogue_poison_t( "venomous_wound", 79136, p )
        {
          proc = true;
          direct_power_mod = .1125; // XXX: it has extra_coeff() but do no match
          reset();
        }
      };
      p -> active_venomous_wound = new venomous_wound_t( p );
    }

    p -> procs_venomous_wounds -> occur();
    p -> active_venomous_wound -> execute();
    
    // Venomous Vim (51637) lists only 2 energy for some reason
    p -> resource_gain( RESOURCE_ENERGY, 10, p -> gains_venomous_vim );
  }
}

// apply_poison_debuff ======================================================

static void apply_poison_debuff( rogue_t* p )
{
  target_t* t = p -> sim -> target;

  if ( p -> talents.master_poisoner -> rank() ) 
    t -> debuffs.master_poisoner -> increment();

  if ( p -> talents.savage_combat -> rank() ) 
    t -> debuffs.savage_combat -> increment();

  t -> debuffs.poisoned -> increment();
}

// remove_poison_debuff =====================================================

static void remove_poison_debuff( rogue_t* p )
{
  target_t* t = p -> sim -> target;

  if ( p -> talents.master_poisoner -> rank() ) 
    t -> debuffs.master_poisoner -> decrement();

  if ( p -> talents.savage_combat -> rank() ) 
    t -> debuffs.savage_combat -> decrement();

  t -> debuffs.poisoned -> decrement();
}

// =========================================================================
// Attacks
// =========================================================================

double rogue_attack_t::armor() SC_CONST
{
  rogue_t* p = player -> cast_rogue();

  double a = attack_t::armor();

  if ( p -> buffs_find_weakness -> check() )
    a *= 1.0 - p -> buffs_find_weakness -> value();

  return a;
}

// rogue_attack_t::_init ===================================================

void rogue_attack_t::_init_rogue_attack_t()
{
  may_crit       = true;
  tick_may_crit = true;
  hasted_ticks  = false;

  // reset some damage related stuff ::parse_data set which we will overwhire anyway in actual spell ctors/executes
  direct_power_mod = 0.0;
  tick_power_mod   = 0.0;

  requires_weapon       = WEAPON_NONE;
  requires_position     = POSITION_NONE;
  requires_stealth      = false;
  requires_combo_points = false;

  adds_combo_points     = (int) player -> player_data.effect_base_value( id, E_ADD_COMBO_POINTS );

  combo_points_spent    = 0;
    
  rogue_t* p = player -> cast_rogue();

  base_hit  += p -> talents.precision -> base_value( E_APPLY_AURA, A_MOD_HIT_CHANCE );
}

// rogue_attack_t::parse_options ===========================================

void rogue_attack_t::parse_options( option_t*          options, 
                                    const std::string& options_str )
{
  attack_t::parse_options( options, options_str );
}

// rogue_attack_t::parse_options ===========================================

void rogue_attack_t::parse_options( const std::string& options_str )
{
  option_t options[] =
  {
    { NULL, OPT_UNKNOWN, NULL }
  };
  parse_options( options, options_str );
}

// rogue_attack_t::cost ====================================================

double rogue_attack_t::cost() SC_CONST
{
  double c = attack_t::cost();

  if ( c <= 0 ) 
    return 0;

  return c;
}

// rogue_attack_t::consume_resource ========================================

void rogue_attack_t::consume_resource()
{
  attack_t::consume_resource();

  rogue_t* p = player -> cast_rogue();

  combo_points_spent = 0;

  if ( result_is_hit() )
  {
    trigger_relentless_strikes( this );
    
    if ( requires_combo_points )
    {
      combo_points_spent = p -> combo_points -> count;

      p -> combo_points -> clear( name() );
    }

    trigger_ruthlessness( this );
  }
  else
    trigger_energy_refund( this );
}

// rogue_attack_t::execute =================================================

void rogue_attack_t::execute()
{
  rogue_t* p = player -> cast_rogue();

  if ( harmful )
    break_stealth( p );
  
  // simple actions do not invoke any base ::execute()
  // nor any triggers
  if ( simple )
  {
    // if we have a buff attached to the action, trigger it now
    trigger_buff();
    
    consume_resource();
    
    if ( sim -> log )
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    add_combo_points();

    update_ready();
  }
  else
  {
    attack_t::execute();

    if ( result_is_hit() )
    {
      add_combo_points();
      
      if ( result == RESULT_CRIT )
        trigger_seal_fate( this );

      // attached buff get's triggered only on successfull hit
      trigger_buff();

      trigger_apply_poisons( this );
      trigger_main_gauche( this );

      trigger_tricks_of_the_trade( this );
    }

    trigger_tier10_4pc( this ); // FIX-ME: Does it require the finishing move to hit before it can proc?

    // Prevent non-critting abilities (Rupture, Garrote, Shiv; maybe something else) from eating Cold Blood
    if ( may_crit ) 
      p -> buffs_cold_blood -> expire();
  }
}

// rogue_attack_t::calculate_weapon_damage ================================

double rogue_attack_t::calculate_weapon_damage()
{
  double dmg = attack_t::calculate_weapon_damage();
  
  if( dmg == 0 ) return 0;

  rogue_t* p = player -> cast_rogue();

  if ( weapon -> slot == SLOT_OFF_HAND && p -> spec_ambidexterity -> ok() )
  {
    dmg *= 1.0 + p -> spec_ambidexterity -> base_value( E_APPLY_AURA, A_MOD_OFFHAND_DAMAGE_PCT );
  }

  return dmg;
}

// rogue_attack_t::player_buff ============================================

void rogue_attack_t::player_buff()
{
  attack_t::player_buff();
  
  rogue_t* p = player -> cast_rogue();

  if ( school == SCHOOL_PHYSICAL )
    player_multiplier *= 1.0 + p -> spec_assassins_resolve -> base_value( E_APPLY_AURA, A_MOD_DAMAGE_PERCENT_DONE );

  if ( p -> buffs_cold_blood -> check() )
    player_crit += p -> buffs_cold_blood -> base_value() / 100.0;
}

// rogue_attack_t::total_multiplier =======================================

double rogue_attack_t::total_multiplier() SC_CONST
{
  // we have to overwrite it because Executioner is additive with talents
  rogue_t* p = player -> cast_rogue();

  double add_mult = 0.0;
  
  if ( requires_combo_points && p -> mastery_executioner -> ok() )
    add_mult = p -> composite_mastery() * p -> mastery_executioner -> base_value( E_APPLY_AURA, A_DUMMY );
  
  return ( base_multiplier + add_mult ) * player_multiplier * target_multiplier; 
}

// rogue_attack_t::ready() ================================================

bool rogue_attack_t::ready()
{
  if ( ! attack_t::ready() )
    return false;

  rogue_t* p = player -> cast_rogue();

  if ( requires_weapon != WEAPON_NONE )
    if ( ! weapon || weapon -> type != requires_weapon )
      return false;

  if ( requires_position != POSITION_NONE )
    if ( p -> position != requires_position )
      return false;

  if ( requires_combo_points && ! p -> combo_points -> count )
    return false;

  if ( requires_stealth )
  {
    if ( ! p -> buffs_shadow_dance -> check() && 
         ! p -> buffs_stealthed -> check() && 
         ! p -> buffs_vanish -> check() )
    {
      return false;
    }
  }

  return true;
}

// rogue_attack_t::assess_damage ===========================================

void rogue_attack_t::assess_damage( double amount,
                                    int    dmg_type )
{
  attack_t::assess_damage( amount, dmg_type );

  rogue_t* p = player -> cast_rogue();

  // XXX: review, as not all of the damage is 'flurried' to an additional target
  // dots for example don't as far as I remember
  if ( p -> buffs_blade_flurry -> up() && target -> adds_nearby )
    attack_t::additional_damage( amount, dmg_type );
}

// rogue_attack_t::add_combo_points ========================================

void rogue_attack_t::add_combo_points()
{
  if ( adds_combo_points > 0 )
  {
    rogue_t* p = player -> cast_rogue();

    p -> combo_points -> add( adds_combo_points, name() );
  }
}

// rogue_attack_t::add_trigger_buff ========================================

void rogue_attack_t::add_trigger_buff( buff_t* buff )
{
  if ( buff )
    m_buff = buff;
}

// rogue_attack_t::trigger_buff ===========================================

void rogue_attack_t::trigger_buff()
{
  if ( m_buff )
    m_buff -> trigger();
}

// Melee Attack ============================================================

struct melee_t : public rogue_attack_t
{
  int sync_weapons;

  melee_t( const char* name, rogue_t* p, int sw ) :
    rogue_attack_t( name, p ), sync_weapons( sw )
  {
    base_dd_min = base_dd_max = 1;
    background      = true;
    repeating       = true;
    trigger_gcd     = 0;
    base_cost       = 0;
    may_crit        = true;

    if ( p -> dual_wield() )
      base_hit -= 0.19;
  }

  virtual double haste() SC_CONST
  {
    rogue_t* p = player -> cast_rogue();

    double h = rogue_attack_t::haste();

    if ( p -> talents.lightning_reflexes -> rank() )
      h *= 1.0 / ( 1.0 + p -> talents.lightning_reflexes -> effect_base_value( 2 ) / 100.0 );

    if ( p -> buffs_slice_and_dice -> up() ) 
      h *= 1.0 / ( 1.0 + p -> buffs_slice_and_dice -> value() );

    if ( p -> buffs_adrenaline_rush -> up() )
      h *= 1.0 / ( 1.0 + p -> buffs_adrenaline_rush -> value() );

    return h;
  }

  virtual double execute_time() SC_CONST
  {
    double t = rogue_attack_t::execute_time();

    if ( ! player -> in_combat )
      return ( weapon -> slot == SLOT_OFF_HAND ) ? ( sync_weapons ? std::min( t/2, 0.2 ) : t/2 ) : 0.01;

    return t;
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    if ( result_is_hit() )
    {
      rogue_t* p = player -> cast_rogue();

      trigger_combat_potency( this );
      p -> buffs_tier11_4pc -> trigger();
    }
      
  }
};

// Auto Attack =============================================================

struct auto_attack_t : public action_t
{
  int sync_weapons;

  auto_attack_t( rogue_t* p, const std::string& options_str ) :
    action_t( ACTION_OTHER, "auto_attack", p ),
    sync_weapons( 0 )
  {
    option_t options[] =
    {
      { "sync_weapons", OPT_BOOL, &sync_weapons },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );

    assert( p -> main_hand_weapon.type != WEAPON_NONE );

    p -> main_hand_attack = new melee_t( "melee_main_hand", p, sync_weapons );
    p -> main_hand_attack -> weapon = &( p -> main_hand_weapon );
    p -> main_hand_attack -> base_execute_time = p -> main_hand_weapon.swing_time;

    if ( p -> off_hand_weapon.type != WEAPON_NONE )
    {
      p -> off_hand_attack = new melee_t( "melee_off_hand", p, sync_weapons );
      p -> off_hand_attack -> weapon = &( p -> off_hand_weapon );
      p -> off_hand_attack -> base_execute_time = p -> off_hand_weapon.swing_time;
    }

    trigger_gcd = 0;
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    p -> main_hand_attack -> schedule_execute();

    if ( p -> off_hand_attack )
      p -> off_hand_attack -> schedule_execute();
  }

  virtual bool ready()
  {
    rogue_t* p = player -> cast_rogue();

    if ( p -> buffs.moving -> check() ) 
      return false;

    return ( p -> main_hand_attack -> execute_event == 0 ); // not swinging
  }
};

// Adrenaline Rush ==========================================================

struct adrenaline_rush_t : public rogue_attack_t
{
  adrenaline_rush_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "adrenaline_rush", p -> talents.adrenaline_rush -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_adrenaline_rush );

    parse_options( options_str );
  }
};

// Ambush ==================================================================

struct ambush_t : public rogue_attack_t
{
  ambush_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "ambush", 8676, p )
  {
    requires_position      = POSITION_BACK;
    requires_stealth       = true;

    if ( weapon -> type == WEAPON_DAGGER )
      weapon_multiplier   *= 1.447; // It'is in the description.

    base_cost             += p -> talents.slaughter_from_the_shadows -> effect_base_value( 1 );
    base_multiplier       *= 1.0 + ( p -> talents.improved_ambush -> mod_additive( P_GENERIC ) +
                                     p -> talents.opportunity     -> mod_additive( P_GENERIC ) );
    base_crit             += p -> talents.improved_ambush -> mod_additive( P_CRIT );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

     rogue_t* p = player -> cast_rogue();

    if ( result_is_hit() )
    {
      trigger_initiative( this );
      trigger_find_weakness( this );
    }

    p -> buffs_shadowstep -> expire();
  }

  virtual void player_buff()
  {
    rogue_attack_t::player_buff();

    rogue_t* p = player -> cast_rogue();

    if ( p -> buffs_shadowstep -> check() )
      player_multiplier *= 1.0 + p -> buffs_shadowstep -> value();
  }
};

// Backstab ==================================================================

struct backstab_t : public rogue_attack_t
{
  backstab_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "backstab", 53, p )
  {
    requires_weapon    = WEAPON_DAGGER;
    requires_position  = POSITION_BACK;
    
    weapon_multiplier *= 1.0 + p -> spec_sinister_calling -> mod_additive( P_EFFECT_2 );

    base_cost         += p -> talents.slaughter_from_the_shadows -> effect_base_value( 1 );
    base_multiplier   *= 1.0 + ( p -> talents.aggression  -> mod_additive( P_GENERIC ) +
                                 p -> talents.opportunity -> mod_additive( P_GENERIC ) );
    base_crit         += p -> talents.puncturing_wounds -> effect_base_value( 1 ) / 100.0;
    base_crit_bonus_multiplier *= 1.0 + p -> talents.lethality -> mod_additive( P_CRIT_DAMAGE );

    if ( p -> set_bonus.tier11_2pc_melee() )
      base_crit += .05;

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    rogue_t* p = player -> cast_rogue();

    if ( result_is_hit() )
    {
      if ( p -> glyphs.backstab -> ok() && result == RESULT_CRIT )
      {
        double amount = p -> glyphs.backstab -> base_value( E_APPLY_AURA );
        p -> resource_gain( RESOURCE_ENERGY, amount, p -> gains_backstab_glyph );
      }
            
      if ( p -> talents.murderous_intent -> rank() )
      {
        // the first effect of the talent is the amount energy gained
        // and the second one is target health percent apparently
        double health_pct = p -> talents.murderous_intent -> effect_base_value( 2 );
        if ( target -> health_percentage() < health_pct )
        {
          double amount = p -> talents.murderous_intent -> effect_base_value( 1 );
          p -> resource_gain( RESOURCE_ENERGY, amount, p -> gains_murderous_intent );
        }
      }
    }
  }
};

// Blade Flurry ============================================================

struct blade_flurry_t : public rogue_attack_t
{
  blade_flurry_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "blade_flurry", p -> spec_blade_flurry -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_blade_flurry );

    parse_options( options_str );
  }
};

// Cold Blood ==============================================================

struct cold_blood_t : public rogue_attack_t
{
  cold_blood_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "cold_blood", p -> talents.cold_blood -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_cold_blood );

    harmful = false;

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();
    
    rogue_t* p = player -> cast_rogue();

    p -> resource_gain( RESOURCE_ENERGY, base_value( E_ENERGIZE ), p -> gains_cold_blood );
  }
};

// Envenom ==================================================================

struct envenom_t : public rogue_attack_t
{
  double dose_dmg;

  envenom_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "envenom", 32645, p )
  {
    // to trigger poisons
    weapon = &( p -> main_hand_weapon );
    weapon_multiplier = 0;

    requires_combo_points = true;

    base_multiplier *= 1.0 + p -> talents.coup_de_grace -> mod_additive( P_GENERIC );

    dose_dmg = effect_average( 1 );

    option_t options[] =
    {
      { "min_doses", OPT_DEPRECATED, NULL },
      { "no_buff",   OPT_DEPRECATED, NULL },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    p -> buffs_envenom -> trigger( p -> combo_points -> count );

    rogue_attack_t::execute();

    if ( result_is_hit() )
    {
      int doses_consumed = std::min( p -> buffs_poison_doses -> current_stack, combo_points_spent );
      
      if ( ! p -> talents.master_poisoner -> rank() )
        p -> buffs_poison_doses -> decrement( doses_consumed );

      if ( ! p -> buffs_poison_doses -> check() )
        p -> active_deadly_poison -> cancel();

      trigger_cut_to_the_chase( this );
      trigger_restless_blades( this );

      p -> buffs_revealing_strike -> expire();
      p -> buffs_tier11_4pc -> expire();
    }
  }

  virtual void player_buff()
  {
    rogue_t* p = player -> cast_rogue();

    int doses_consumed = std::min( p -> buffs_poison_doses -> current_stack,
                                   p -> combo_points -> count );

    direct_power_mod = p -> combo_points -> count * 0.09;

    base_dd_min = base_dd_max = doses_consumed * dose_dmg;

    rogue_attack_t::player_buff();

    if ( p -> buffs_revealing_strike -> up() )
      player_multiplier *= 1.0 + p -> buffs_revealing_strike -> value();

    if ( p -> buffs_tier11_4pc -> up() )
      player_crit += 1.0;
  }

  virtual double total_multiplier() SC_CONST
  {
    // we have to overwrite it because Potent Poisons is additive with talents
    rogue_t* p = player -> cast_rogue();

    double add_mult = 0.0;
  
    if ( p -> mastery_potent_poisons -> ok() )
      add_mult = p -> composite_mastery() * p -> mastery_potent_poisons -> base_value( E_APPLY_AURA, A_DUMMY );
  
    return ( base_multiplier + add_mult ) * player_multiplier * target_multiplier; 
  }

  virtual bool ready()
  {
    rogue_t* p = player -> cast_rogue();

    // Envenom is not usable when there is no DP on a target
    if ( ! p -> buffs_poison_doses -> check() )
      return false;

    return rogue_attack_t::ready();
  }
};

// Eviscerate ================================================================

struct eviscerate_t : public rogue_attack_t
{
  double combo_point_dmg_min[ COMBO_POINTS_MAX ];
  double combo_point_dmg_max[ COMBO_POINTS_MAX ];

  eviscerate_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "eviscerate", 2098, p )
  {
    // to trigger poisons
    weapon = &( p -> main_hand_weapon );
    weapon_multiplier = 0;

    requires_combo_points = true;

    base_multiplier *= 1.0 + ( p -> talents.aggression    -> mod_additive( P_GENERIC ) +
                               p -> talents.coup_de_grace -> mod_additive( P_GENERIC ) );
    base_crit       += p -> glyphs.eviscerate -> mod_additive( P_CRIT );

    double _min     = effect_min( 1 );
    double _max     = effect_max( 1 );
    double cp_bonus = effect_bonus( 1 );

    for (int i = 0; i < COMBO_POINTS_MAX; i++)
    {
      combo_point_dmg_min[ i ] = _min + cp_bonus * ( i + 1 );
      combo_point_dmg_max[ i ] = _max + cp_bonus * ( i + 1 );
    }

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    base_dd_min = p -> combo_points -> rank( combo_point_dmg_min );
    base_dd_max = p -> combo_points -> rank( combo_point_dmg_max );

    direct_power_mod = 0.091 * p -> combo_points -> count;

    rogue_attack_t::execute();

    if ( result_is_hit() )
    {
      trigger_cut_to_the_chase( this );
      trigger_restless_blades( this );

      if ( p -> talents.serrated_blades-> rank() && 
           p -> dots_rupture -> ticking )
      {
        double chance = p -> talents.serrated_blades -> base_value() / 100.0 * combo_points_spent;
        if ( p -> rng_serrated_blades -> roll( chance ) )
        {
          p -> dots_rupture -> action -> refresh_duration();
          p -> procs_serrated_blades -> occur();
        }
      }

      p -> buffs_revealing_strike -> expire();
      p -> buffs_tier11_4pc -> expire();
    }
  }

  virtual void player_buff()
  {
    rogue_attack_t::player_buff();

    rogue_t* p = player -> cast_rogue();

    if ( p -> buffs_revealing_strike -> up() )
      player_multiplier *= 1.0 + p -> buffs_revealing_strike -> value();
    
    if ( p -> buffs_tier11_4pc -> up() )
      player_crit += 1.0;
  }
};

// Expose Armor =============================================================

struct expose_armor_t : public rogue_attack_t
{
  int override_sunder;

  expose_armor_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "expose_armor", 8647, p ), override_sunder( 0 )
  {
    // to trigger poisons
    weapon = &( p -> main_hand_weapon );
    weapon_multiplier = 0;

    requires_combo_points = true;

    option_t options[] =
    {
      { "override_sunder", OPT_BOOL, &override_sunder },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    rogue_t* p = player -> cast_rogue();

    if ( result_is_hit() )
    {
      double duration = 10 * combo_points_spent;
      
      duration += p -> glyphs.expose_armor -> mod_additive( P_DURATION );

      if ( p -> buffs_revealing_strike -> up() )
        duration *= 1.0 + p -> buffs_revealing_strike -> value();

      if ( target -> debuffs.expose_armor -> remains_lt( duration ) )
      {
        target -> debuffs.expose_armor -> buff_duration = duration;
        target -> debuffs.expose_armor -> trigger( 1, 0.12 );
        target -> debuffs.expose_armor -> source = p;
      }

      if ( p -> talents.improved_expose_armor -> rank() )
      {
        double chance = p -> talents.improved_expose_armor -> rank() * 0.50; // XXX
        if ( p -> rng_improved_expose_armor -> roll( chance ) )
          p -> combo_points -> add( combo_points_spent, p -> talents.improved_expose_armor );
      }

      p -> buffs_revealing_strike -> expire();
    }
  };

  virtual bool ready()
  {

    if ( target -> debuffs.expose_armor -> check() )
      return false;

    if ( ! override_sunder )
    {
      if ( target -> debuffs.sunder_armor -> check() ||
           target -> debuffs.faerie_fire -> check() )
      {
        return false;
      }
    }

    return rogue_attack_t::ready();
  }
};

// Feint ====================================================================

struct feint_t : public rogue_attack_t
{
  feint_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "feint", 1966, p )
  {
    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log ) 
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    consume_resource();
    update_ready();
    // model threat eventually......
  }
};

// Garrote ==================================================================

struct garrote_t : public rogue_attack_t
{
  garrote_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "garrote", 703, p )
  {
    // to trigger poisons
    weapon = &( p -> main_hand_weapon );
    weapon_multiplier = 0;

    // to stop seal fate procs
    may_crit          = false;

    requires_position = POSITION_BACK;
    requires_stealth  = true;

    tick_power_mod    = .07;

    base_multiplier  *= 1.0 + p -> talents.opportunity -> mod_additive( P_TICK_DAMAGE );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    rogue_t* p = player -> cast_rogue();

    if ( result_is_hit() )
    {
      trigger_initiative( this );
      trigger_find_weakness( this );
    }

    p -> buffs_shadowstep -> expire();
  }

  virtual void player_buff()
  {
    rogue_attack_t::player_buff();

    rogue_t* p = player -> cast_rogue();

    if ( p -> buffs_shadowstep -> check() )
      player_multiplier *= 1.0 + p -> buffs_shadowstep -> value();
  }

  virtual void tick()
  {
    rogue_attack_t::tick();

    trigger_venomous_wounds( this );
  }
};

// Hemorrhage =============================================================

struct hemorrhage_t : public rogue_attack_t
{
  hemorrhage_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "hemorrhage", p -> talents.hemorrhage -> spell_id(), p )
  {
    // It does not have CP gain effect in spell dbc for soem reason
    adds_combo_points = 1;

    if ( weapon -> type == WEAPON_DAGGER )
      weapon_multiplier *= 1.45; // number taken from spell description

    weapon_multiplier *= 1.0 + p -> spec_sinister_calling -> mod_additive( P_EFFECT_2 );

    base_cost       += p -> talents.slaughter_from_the_shadows -> effect_base_value( 2 );
    base_crit_bonus_multiplier *= 1.0 + p -> talents.lethality -> mod_additive( P_CRIT_DAMAGE );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    rogue_t* p = player -> cast_rogue();
    
    if ( result_is_hit() )
    {
      target -> debuffs.hemorrhage -> trigger();
      target -> debuffs.hemorrhage -> source = p;

      if ( p -> glyphs.hemorrhage -> ok() )
      {
        // XXX Add glyph here
      }
    }
  }
};

// Kick =====================================================================

struct kick_t : public rogue_attack_t
{
  kick_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "kick", 1766, p )
  {
    base_dd_min = base_dd_max = 1;
    may_miss = may_resist = may_glance = may_block = may_dodge = may_crit = false;
    base_attack_power_multiplier = 0.0;

    parse_options( options_str );
  }

  virtual bool ready()
  {
    if ( ! target -> debuffs.casting -> check() )
      return false;

    return rogue_attack_t::ready();
  }
};

// Killing Spree ============================================================

struct killing_spree_tick_t : public rogue_attack_t
{
  killing_spree_tick_t( rogue_t* p ) :
    rogue_attack_t( "killing_spree", p, true )
  {
    dual        = true;
    background  = true;
    may_crit    = true;
    direct_tick = true;
    normalize_weapon_speed = true;
    base_dd_min = base_dd_max = 1;
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    tick_dmg = direct_dmg;

    update_stats( DMG_OVER_TIME );
  }
};

struct killing_spree_t : public rogue_attack_t
{
  attack_t* killing_spree_tick;

  killing_spree_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "killing_spree", p -> talents.killing_spree -> spell_id(), p )
  {
    add_trigger_buff( p -> buffs_killing_spree );
    
    base_dd_min = base_dd_max = 1;
    //channeled = true;
    num_ticks = 5;
    base_tick_time = effect_period( 1 );

    killing_spree_tick = new killing_spree_tick_t( p );

    parse_options( options_str );
  }

  virtual void tick()
  {
    if ( sim -> debug ) log_t::output( sim, "%s ticks (%d of %d)", name(), dot -> current_tick, dot -> num_ticks );

    killing_spree_tick -> weapon = &( player -> main_hand_weapon );
    killing_spree_tick -> execute();

    if ( killing_spree_tick -> result_is_hit() )
    {
      if ( player -> off_hand_weapon.type != WEAPON_NONE )
      {
        killing_spree_tick -> weapon = &( player -> off_hand_weapon );
        killing_spree_tick -> execute();
      }
    }

    update_time( DMG_OVER_TIME );
  }

  // Killing Spree not modified by haste effects
  virtual double haste() SC_CONST { return 1.0; }
};

// Mutilate =================================================================

struct mutilate_strike_t : public rogue_attack_t
{
  mutilate_strike_t( rogue_t* p ) :
    rogue_attack_t( "mutilate", 5374, p )
  {
    dual        = true;
    background  = true;

    base_multiplier  *= 1.0 + p -> talents.opportunity -> mod_additive( P_GENERIC );
    base_crit        += p -> talents.puncturing_wounds -> effect_base_value( 2 ) / 100.0;
    base_crit_bonus_multiplier *= 1.0 + p -> talents.lethality -> mod_additive( P_CRIT_DAMAGE );

    if ( p -> set_bonus.tier11_2pc_melee() )
      base_crit += .05;
  }

  virtual void execute()
  {
    rogue_attack_t::execute();
    update_stats( DMG_DIRECT );
  }

  virtual void player_buff()
  {
    rogue_attack_t::player_buff();

    rogue_t* p = player -> cast_rogue();

    if ( target -> debuffs.poisoned )
      player_multiplier *= 1.20; // XXX: I'm sure it's there somehwere; or not

    p -> uptimes_poisoned -> update( target -> debuffs.poisoned > 0 );
  }
};

struct mutilate_t : public rogue_attack_t
{
  attack_t* strike;
  
  mutilate_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "mutilate", p -> spec_mutilate -> spell_id(), p )
  {
    strike = new mutilate_strike_t( p );

    // so it passes ready checks
    weapon = &( player -> main_hand_weapon );
    requires_weapon = WEAPON_DAGGER;

    base_cost += p -> glyphs.mutilate -> mod_additive( P_RESOURCE_COST );

    parse_options( options_str );
  }

  virtual void execute()
  {    
    rogue_t* p = player -> cast_rogue();

    result = RESULT_HIT;

    consume_resource();
    
    strike -> weapon = &( p -> main_hand_weapon );
    strike -> execute();

    if ( strike -> result_is_hit() )
    {
      int main_hand_result = strike -> result;
      
      strike -> weapon = &( p -> off_hand_weapon );
      strike -> execute();
      
      add_combo_points();

      if ( main_hand_result == RESULT_CRIT || strike -> result == RESULT_CRIT )
        trigger_seal_fate( this );
    }
    else
      trigger_energy_refund( this );

    update_ready();
  }

  virtual bool ready()
  {
    // Mutilate NEEDS a dagger in the off-hand!
    if ( player -> off_hand_weapon.type != WEAPON_DAGGER )
      return false;

    return rogue_attack_t::ready();
  }
};

// Premeditation =============================================================

struct premeditation_t : public rogue_attack_t
{
  premeditation_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "premeditation", p -> talents.premeditation -> spell_id(), p, true )
  {
    harmful = false;
    requires_stealth = true;

    parse_options( options_str );
  }
};

// Recuperate ================================================================

struct recuperate_t : public rogue_attack_t
{
  recuperate_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "recuperate", 73651, p )
  {
    requires_combo_points = true;

    base_tick_time = effect_period( 1 );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log )
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    num_ticks = 2 * p -> combo_points -> count;

    p -> buffs_recuperate -> buff_duration = num_ticks * base_tick_time;
    p -> buffs_recuperate -> trigger();
    
    consume_resource();

    schedule_travel();
  }

  virtual void tick()
  {
    rogue_t* p = player -> cast_rogue();

    if ( p -> talents.energetic_recovery -> rank() )
      p -> resource_gain( RESOURCE_ENERGY, p -> talents.energetic_recovery -> base_value(), p -> gains_recuperate );
  }

  virtual bool ready()
  {
    rogue_t* p = player -> cast_rogue();

    // do not allow execution when the buff is active for now
    if ( p -> buffs_recuperate -> check() )
      return false;

    return rogue_attack_t::ready();
  }
};

// Revealing Strike ==========================================================

struct revealing_strike_t : public rogue_attack_t
{
  revealing_strike_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "revealing_strike", p -> talents.revealing_strike -> spell_id(), p )
  {
    add_trigger_buff( p -> buffs_revealing_strike );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    if ( result_is_hit() )
    {
      trigger_bandits_guile( this );
    }
  }
};

// Rupture ==================================================================

struct rupture_t : public rogue_attack_t
{
  double combo_point_dmg[ COMBO_POINTS_MAX ];

  rupture_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "rupture", 1943, p )
  {
    // to trigger poisons
    weapon = &( p -> main_hand_weapon );
    weapon_multiplier = 0;

    // to stop seal fate procs
    may_crit              = false;
    requires_combo_points = true;
    
    double base     = effect_average( 1 );
    double cp_bonus = effect_bonus( 1 );

    for (int i = 0; i < COMBO_POINTS_MAX; i++)
      combo_point_dmg[ i ] = base + cp_bonus * ( i + 1 );

    parse_options( options_str );
  }

  virtual void execute()
  {
    // We have to save these values for refreshes by Serrated Baldes, so
    // we simply reset them to zeroes on each execute and check them in ::player_buff.
    tick_power_mod = 0.0;
    base_td        = 0.0;
    
    rogue_attack_t::execute();
    
    rogue_t* p = player -> cast_rogue();
    
    if ( result_is_hit() )
    {
      num_ticks = 3 + combo_points_spent + (int)( p -> glyphs.rupture -> mod_additive( P_DURATION ) / base_tick_time );

      update_ready();

      p -> buffs_revealing_strike -> expire();

      trigger_restless_blades( this );
    }
  }

  virtual void player_buff()
  {
    rogue_t* p = player -> cast_rogue();

    // If the values are zeroes, we are called from ::execute and we must update.
    // If the values are initialized, we are called from ::refresh_duration and we don't
    // need to update.
    if ( tick_power_mod == 0.0 && base_td == 0.0 )
    {
      tick_power_mod = p -> combo_points -> rank( 0.015, 0.024, 0.030, 0.03428571, 0.0375 );
      base_td = p -> combo_points -> rank( combo_point_dmg );
    }

    rogue_attack_t::player_buff();

    if ( p -> buffs_revealing_strike -> up() )
      player_multiplier *= 1.0 + p -> buffs_revealing_strike -> value();
  }

  virtual void tick()
  {
    rogue_attack_t::tick();

    trigger_venomous_wounds( this );
  }
};

// Shadowstep ===============================================================

struct shadowstep_t : public rogue_attack_t
{
  shadowstep_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "shadowstep", p -> spec_shadowstep -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_shadowstep );

    harmful = false;

    parse_options( options_str );
  }
};

// Shiv =====================================================================

struct shiv_t : public rogue_attack_t
{
  shiv_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "shiv", 5938, p )
  {
    weapon = &( p -> off_hand_weapon );
    if ( weapon -> type == WEAPON_NONE ) 
      background = true; // Do not allow execution.

    base_cost        += weapon -> swing_time * 10.0;
    adds_combo_points = 1;
    may_crit          = false;
    base_dd_min = base_dd_max = 1;

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    p -> buffs_shiv -> trigger();
    rogue_attack_t::execute();
    p -> buffs_shiv -> expire();
  }
};

// Sinister Strike ==========================================================

struct sinister_strike_t : public rogue_attack_t
{
  sinister_strike_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "sinister_strike", 1752, p )
  {
    adds_combo_points = 1; // it has an effect but with no base value :rollseyes:
    
    base_cost       += p -> talents.improved_sinister_strike -> mod_additive( P_RESOURCE_COST );
    base_multiplier *= 1.0 + ( p -> talents.aggression               -> mod_additive( P_GENERIC ) +
                               p -> talents.improved_sinister_strike -> mod_additive( P_GENERIC ) );
    base_crit_bonus_multiplier *= 1.0 + p -> talents.lethality -> mod_additive( P_CRIT_DAMAGE );

    if ( p -> set_bonus.tier11_2pc_melee() )
      base_crit += .05;

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();

    rogue_t* p = player -> cast_rogue();

    if ( result_is_hit() )
    {
      trigger_bandits_guile( this );
      
      if ( p -> rng_sinister_strike_glyph -> roll( p -> glyphs.sinister_strike -> proc_chance() ) )
      {
        p -> procs_sinister_strike_glyph -> occur();
        p -> combo_points -> add( 1, p -> glyphs.sinister_strike );
      }
    }
  }
};

// Slice and Dice ===========================================================

struct slice_and_dice_t : public rogue_attack_t
{
  slice_and_dice_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "slice_and_dice", 5171, p )
  {
    requires_combo_points = true;

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log )
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    p -> buffs_slice_and_dice -> trigger ( p -> combo_points -> count );

    consume_resource();
  }
};

// Pool Energy ==============================================================

struct pool_energy_t : public action_t
{
  double wait;
  int for_next;

  pool_energy_t( rogue_t* p, const std::string& options_str ) :
    action_t( ACTION_OTHER, "pool_energy", p ),
    wait( 0.5 ), for_next( 0 )
  {
    option_t options[] =
    {
      { "wait",     OPT_FLT,  &wait     },
      { "for_next", OPT_BOOL, &for_next },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );
  }

  virtual void execute()
  {
    if ( sim -> log ) 
      log_t::output( sim, "%s performs %s", player -> name(), name() );
  }

  virtual double gcd() SC_CONST
  {
    return wait;
  }

  virtual bool ready()
  {
    if ( for_next )
    {
      if ( next -> ready() )
        return false;

      // If the next action in the list would be "ready" if it was not constrained by energy,
      // then this command will pool energy until we have enough.

      player -> resource_current[ RESOURCE_ENERGY ] += 100;

      bool energy_limited = next -> ready();

      player -> resource_current[ RESOURCE_ENERGY ] -= 100;

      if ( ! energy_limited ) 
        return false;
    }

    return action_t::ready();
  }
};

// Preparation ==============================================================

struct preparation_t : public rogue_attack_t
{
  std::vector<cooldown_t*> cooldown_list;

  preparation_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "preparation", p -> talents.preparation -> spell_id(), p, true )
  {
    harmful = false;
    
    cooldown_list.push_back( p -> get_cooldown( "shadowstep" ) );
    cooldown_list.push_back( p -> get_cooldown( "vanish"     ) );

    if ( p -> glyphs.preparation -> ok() )
      cooldown_list.push_back( p -> get_cooldown( "kick" ) );

    parse_options( options_str );
  }

  virtual void execute()
  {
    rogue_attack_t::execute();
    
    int num_cooldowns = (int) cooldown_list.size();
    for ( int i = 0; i < num_cooldowns; i++ )
      cooldown_list[ i ] -> reset();
  }
};

// Shadow Dance =============================================================

struct shadow_dance_t : public rogue_attack_t
{
  shadow_dance_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "shadow_dance", p -> talents.shadow_dance -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_shadow_dance );

    p -> buffs_shadow_dance -> buff_duration += p -> glyphs.shadow_dance -> mod_additive( P_DURATION );

    parse_options( options_str );
  }
};

// Tricks of the Trade =======================================================

struct tricks_of_the_trade_t : public rogue_attack_t
{
  player_t* tricks_target;
  bool unspecified_target;

  tricks_of_the_trade_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "tricks_target", 57934, p ),
    unspecified_target( false )
  {
    std::string target_str = p -> tricks_of_the_trade_target_str;

    option_t options[] =
    {
      { "target", OPT_STRING, &target_str },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );

    if ( p -> set_bonus.tier10_2pc_melee() ) 
      base_cost = 0;

    if ( p -> glyphs.tricks_of_the_trade -> ok() )
      base_cost = 0;

    if ( target_str.empty() || target_str == "none" )
      tricks_target = 0;
    else if ( target_str == "other" )
    {
      tricks_target = 0;
      unspecified_target = true;
    }
    else if ( target_str == "self" )
      tricks_target = p;
    else
    {
      tricks_target = sim -> find_player( target_str );

      assert ( tricks_target != 0 );
      assert ( tricks_target != player );
    }
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log )
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    consume_resource();

    if ( p -> set_bonus.tier10_2pc_melee() )
      p -> resource_gain( RESOURCE_ENERGY, 15, p -> gains_tier10_2pc );

    update_ready();

    if ( ! unspecified_target )
    {
      if ( sim -> log )
        log_t::output( sim, "%s grants %s Tricks of the Trade", p -> name(), tricks_target -> name() );

      p -> tricks_of_the_trade_target = tricks_target;
    }
  }

  virtual bool ready()
  {
    if ( ! unspecified_target )
    {
      if ( ! tricks_target )
        return false;

      if ( tricks_target -> buffs.tricks_of_the_trade -> check() )
        return false;
    }

    return rogue_attack_t::ready();
  }
};

// Vanish ===================================================================

struct vanish_t : public rogue_attack_t
{
  vanish_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "vanish", 1856, p, true )
  {
    add_trigger_buff( p -> buffs_vanish );

    harmful = false;

    cooldown -> duration += p -> talents.elusiveness -> effect_base_value( 1 ) / 1000.0;

    parse_options( options_str );
  }

  virtual bool ready()
  {
    rogue_t* p = player -> cast_rogue();

    if ( p -> buffs_stealthed -> check() )
      return false;

    return rogue_attack_t::ready();
  }
};

// Vendetta =================================================================

struct vendetta_t : public rogue_attack_t
{
  vendetta_t( rogue_t* p, const std::string& options_str ) :
    rogue_attack_t( "vendetta", p -> talents.vendetta -> spell_id(), p, true )
  {
    add_trigger_buff( p -> buffs_vendetta );

    harmful = false;
    
    parse_options( options_str );
  }
};

// =========================================================================
// Poisons
// =========================================================================

// rogue_poison_t::total_multiplier =======================================

double rogue_poison_t::total_multiplier() SC_CONST
{
  // we have to overwrite it because Potent Poisons is additive with talents
  rogue_t* p = player -> cast_rogue();

  double add_mult = 0.0;
  
  if ( p -> mastery_potent_poisons -> ok() )
    add_mult = p -> composite_mastery() * p -> mastery_potent_poisons -> base_value( E_APPLY_AURA, A_DUMMY );
  
  return ( base_multiplier + add_mult ) * player_multiplier * target_multiplier; 
}

// Deadly Poison ============================================================

struct deadly_poison_t : public rogue_poison_t
{
  deadly_poison_t( rogue_t* player ) :  rogue_poison_t( "deadly_poison", 2818, player )
  {
    tick_power_mod = extra_coeff();
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    int success = p -> buffs_shiv -> check() || p -> buffs_deadly_proc -> check();

    if ( ! success )
    {
      double chance = 0.30;

      chance += p -> spec_improved_poisons -> mod_additive( P_PROC_CHANCE );

      if ( p -> buffs_envenom -> up() )
        chance += p -> buffs_envenom -> base_value( E_APPLY_AURA, A_ADD_FLAT_MODIFIER ) / 100.0;

      success = p -> rng_deadly_poison -> roll( chance );
    }

    if ( success )
    {
      p -> procs_deadly_poison -> occur();

      player_buff();
      target_debuff( DMG_DIRECT );
      calculate_result();

      if ( result_is_hit() )
      {
        if ( p -> buffs_poison_doses -> check() == ( int ) max_stacks() )
        {
          p -> buffs_deadly_proc -> trigger();
          weapon_t* other_w = ( weapon -> slot == SLOT_MAIN_HAND ) ? &( p -> off_hand_weapon ) : &( p -> main_hand_weapon );
          trigger_other_poisons( p, other_w );
          p -> buffs_deadly_proc -> expire();
        }

        p -> buffs_poison_doses -> trigger();

        if ( sim -> log )
          log_t::output( sim, "%s performs %s (%d)", player -> name(), name(), p -> buffs_poison_doses -> current_stack );

        if ( dot -> ticking )
        {
          refresh_duration();
        }
        else
        {
          schedule_travel();
          apply_poison_debuff( p );
        }
      }
    }
  }

  virtual double calculate_tick_damage()
  {
    rogue_t* p = player -> cast_rogue();
    return rogue_poison_t::calculate_tick_damage() * p -> buffs_poison_doses -> stack();
  }

  virtual void last_tick()
  {
    rogue_poison_t::last_tick();

    rogue_t* p = player -> cast_rogue();

    p -> buffs_poison_doses -> expire();
    remove_poison_debuff( p );
  }
};

// Instant Poison ===========================================================

struct instant_poison_t : public rogue_poison_t
{
  instant_poison_t( rogue_t* player ) : rogue_poison_t( "instant_poison", 8680, player )
  {
    direct_power_mod = extra_coeff();
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();
    double chance;

    if ( p -> buffs_deadly_proc -> check() )
    {
      chance = 1.0;
      may_crit = true;
    }
    else if ( p -> buffs_shiv -> check() )
    {
      chance = 1.0;
      may_crit = false;
    }
    else
    {
      double m = 1.0;

      m += p -> spec_improved_poisons -> mod_additive( P_PROC_FREQUENCY );

      if ( p -> buffs_envenom -> up() )
        m += p -> buffs_envenom -> base_value( E_APPLY_AURA, A_ADD_PCT_MODIFIER );

      chance = weapon -> proc_chance_on_swing( 8.57 * m );
      may_crit = true;
    }

    if ( p -> rng_instant_poison -> roll( chance ) )
      rogue_poison_t::execute();
  }
};

// Wound Poison ============================================================

struct wound_poison_t : public rogue_poison_t
{
  wound_poison_t( rogue_t* player ) : rogue_poison_t( "wound_poison", 13218, player )
  {
    direct_power_mod = extra_coeff();
  }

  virtual void execute()
  {
    struct expiration_t : public event_t
    {
      expiration_t( sim_t* sim, rogue_t* p ) : event_t( sim, p )
      {
        name = "Wound Poison Expiration";
        apply_poison_debuff( p );
        sim -> add_event( this, 15.0 );
      }
      virtual void execute()
      {
        rogue_t* p = player -> cast_rogue();
        p -> _expirations.wound_poison = 0;
        remove_poison_debuff( p );
      }
    };

    rogue_t* p = player -> cast_rogue();
    double chance;

    if ( p -> buffs_deadly_proc -> check() )
    {
      chance = 1.0;
      may_crit = true;
    }
    else if ( p -> buffs_shiv -> check() )
    {
      chance = 1.0;
      may_crit = false;
    }
    else
    {
      double PPM = 21.43;
      chance = weapon -> proc_chance_on_swing( PPM );
      may_crit = true;
    }

    if ( p -> rng_wound_poison -> roll( chance ) )
    {
      rogue_poison_t::execute();

      if ( result_is_hit() )
      {
        event_t*& e = p -> _expirations.wound_poison;

        if ( e )
          e -> reschedule( 15.0 );
        else
          e = new ( sim ) expiration_t( sim, p );
      }
    }
  }
};

// Apply Poison ===========================================================

struct apply_poison_t : public action_t
{
  int main_hand_poison;
  int  off_hand_poison;

  apply_poison_t( rogue_t* p, const std::string& options_str ) :
    action_t( ACTION_OTHER, "apply_poison", p ),
    main_hand_poison(0), off_hand_poison(0)
  {
    std::string main_hand_str;
    std::string  off_hand_str;

    option_t options[] =
    {
      { "main_hand", OPT_STRING, &main_hand_str },
      {  "off_hand", OPT_STRING,  &off_hand_str },
      { NULL, OPT_UNKNOWN, NULL }
    };
    parse_options( options, options_str );

    if ( main_hand_str.empty() &&
         off_hand_str.empty() )
    {
      sim -> errorf( "Player %s: At least one of 'main_hand/off_hand' must be specified in 'apply_poison'.  Accepted values are 'deadly/instant/wound'.\n", p -> name() );
      sim -> cancel();
    }

    trigger_gcd = 0;

    if ( p -> main_hand_weapon.type != WEAPON_NONE )
    {
      if ( main_hand_str == "deadly"    ) main_hand_poison =     DEADLY_POISON;
      if ( main_hand_str == "instant"   ) main_hand_poison =    INSTANT_POISON;
      if ( main_hand_str == "wound"     ) main_hand_poison =      WOUND_POISON;
    }

    if ( p -> off_hand_weapon.type != WEAPON_NONE )
    {
      if ( off_hand_str == "deadly"     ) off_hand_poison =     DEADLY_POISON;
      if ( off_hand_str == "instant"    ) off_hand_poison =    INSTANT_POISON;
      if ( off_hand_str == "wound"      ) off_hand_poison =      WOUND_POISON;
    }

    if ( ! p -> active_deadly_poison    ) p -> active_deadly_poison     = new  deadly_poison_t( p );
    if ( ! p -> active_instant_poison   ) p -> active_instant_poison    = new instant_poison_t( p );
    if ( ! p -> active_wound_poison     ) p -> active_wound_poison      = new   wound_poison_t( p );
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log ) 
      log_t::output( sim, "%s performs %s", player -> name(), name() );

    if ( main_hand_poison ) p -> main_hand_weapon.buff_type = main_hand_poison;
    if (  off_hand_poison ) p ->  off_hand_weapon.buff_type =  off_hand_poison;
  };

  virtual bool ready()
  {
    rogue_t* p = player -> cast_rogue();

    if ( p -> main_hand_weapon.type != WEAPON_NONE )
      if ( main_hand_poison )
        return( main_hand_poison != p -> main_hand_weapon.buff_type );

    if ( p -> off_hand_weapon.type != WEAPON_NONE )
      if ( off_hand_poison )
        return( off_hand_poison != p -> off_hand_weapon.buff_type );

    return false;
  }
};

// =========================================================================
// Stealth
// =========================================================================

struct stealth_t : public spell_t
{
  bool used;

  stealth_t( rogue_t* p, const std::string& options_str ) :
    spell_t( "stealth", p ), used(false)
  {
    trigger_gcd = 0;

    id = 1784;
  }

  virtual void execute()
  {
    rogue_t* p = player -> cast_rogue();

    if ( sim -> log )
      log_t::output( sim, "%s performs %s", p -> name(), name() );

    p -> buffs_stealthed -> trigger();
    used = true;
  }

  virtual bool ready()
  {
    return ! used;
  }

  virtual void reset()
  {
    spell_t::reset();
    used = false;
  }
};

// =========================================================================
// Buffs
// =========================================================================

struct adrenaline_rush_buff_t : public new_buff_t
{
  adrenaline_rush_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "adrenaline_rush", id )
  { 
    // we track the cooldown in the actual action
    // and because of restless blades have to remove it here
    cooldown -> duration = 0;
    buff_duration += p -> glyphs.adrenaline_rush -> mod_additive( P_DURATION );
  }

  virtual bool trigger( int, double, double )
  { // we keep haste % as current_value
    return new_buff_t::trigger( 1, base_value( E_APPLY_AURA, A_319 ) );
  }
};

struct envenom_buff_t : public new_buff_t
{
  envenom_buff_t( rogue_t* p ) :
    new_buff_t( p, "envenom", 32645 ) { }

  virtual bool trigger( int cp, double, double )
  {
    double new_duration = 1.0 + cp;

    if ( remains_lt( new_duration ) )
    {
      buff_duration = new_duration;
      return new_buff_t::trigger();
    }
    else
      return false;
  }
};

struct find_weakness_buff_t : public new_buff_t
{
  find_weakness_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "find_weakness", id )
  {
    if ( ! p -> player_data.spell_exists( id ) )
      return;

    // Duration is specified in the actual debuff (or is it a buff?) placed on the target
    buff_duration = p -> player_data.spell_duration( 91021 );

    _init_buff_t();
  }

  virtual bool trigger( int, double, double )
  {
    return new_buff_t::trigger( 1, base_value() / 100.0 );
  }
};

struct killing_spree_buff_t : public new_buff_t
{
  killing_spree_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "killing_spree", id )
  {
    // we track the cooldown in the actual action
    // and because of restless blades have to remove it here
    cooldown -> duration = 0;
  }

  virtual bool trigger( int, double, double )
  {
    rogue_t* p = player -> cast_rogue();

    double value = 0.20; // XXX
    value += p -> glyphs.killing_spree -> mod_additive( P_EFFECT_3 ) / 100.0;

    return new_buff_t::trigger( 1, value );
  }
};

struct master_of_subtlety_buff_t : public new_buff_t
{
  master_of_subtlety_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "master_of_subtlety", id )
  {
    buff_duration = 6.0;
  }

  virtual bool trigger( int, double, double )
  {
    return new_buff_t::trigger( 1, base_value() / 100.0 );
  }
};

struct revealing_strike_buff_t : public new_buff_t
{
  revealing_strike_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "revealing_strike", id ) { }

  virtual bool trigger( int, double, double )
  {
    rogue_t* p = player -> cast_rogue();

    double value = base_value( E_APPLY_AURA, A_DUMMY ) / 100.0;
    value += p -> glyphs.revealing_strike -> mod_additive( P_EFFECT_3 ) / 100.0;
    
    return new_buff_t::trigger( 1, value );
  }
};

struct shadowstep_buff_t : public new_buff_t
{
  shadowstep_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "shadowstep", id ) { }

  virtual bool trigger( int, double, double )
  {
    return new_buff_t::trigger( 1, base_value( E_APPLY_AURA, A_ADD_PCT_MODIFIER ) );
  }
};

struct slice_and_dice_buff_t : public new_buff_t
{
  uint32_t id;
  
  slice_and_dice_buff_t( rogue_t* p ) :
    new_buff_t( p, "slice_and_dice", 5171 ), id( 5171 ) { }

  virtual bool trigger( int cp, double, double )
  {
    rogue_t* p = player -> cast_rogue();
    
    double new_duration = p -> player_data.spell_duration( id );
    new_duration += 3.0 * cp;
    new_duration += p -> glyphs.slice_and_dice -> mod_additive( P_DURATION );
    new_duration *= 1.0 + p -> talents.improved_slice_and_dice -> mod_additive( P_DURATION );

    if ( remains_lt( new_duration ) )
    {
      buff_duration = new_duration;
      return new_buff_t::trigger( 1, base_value( E_APPLY_AURA, A_319 ) );
    }
    else
      return false;
  }
};

struct vendetta_buff_t : public new_buff_t
{
  vendetta_buff_t( rogue_t* p, uint32_t id ) :
    new_buff_t( p, "vendetta", id )
  {
    buff_duration *= 1.0 + p -> glyphs.vendetta -> mod_additive( P_DURATION );
  }

  virtual bool trigger( int, double, double )
  {
    return new_buff_t::trigger( 1, base_value( E_APPLY_AURA, A_MOD_DAMAGE_FROM_CASTER ) );
  }
};

} // ANONYMOUS NAMESPACE ===================================================

// =========================================================================
// Rogue Character Definition
// =========================================================================

// rogue_t::composite_attribute_multiplier ==================================

double rogue_t::composite_attribute_multiplier( int attr ) SC_CONST
{  
  double m = player_t::composite_attribute_multiplier( attr );
  
  if ( attr == ATTR_AGILITY )
    m *= 1.0 + spec_sinister_calling -> base_value( E_APPLY_AURA, A_MOD_TOTAL_STAT_PERCENTAGE );
  
  return m;
}

// rogue_t::matching_gear_multiplier ==================================

double rogue_t::matching_gear_multiplier( const attribute_type attr ) SC_CONST
{  
  if ( attr == ATTR_AGILITY )
    return 0.05;
  
  return 0.0;
}


// rogue_t::composite_attack_power_multiplier ===============================

double rogue_t::composite_attack_power_multiplier() SC_CONST
{
  double m = player_t::composite_attack_power_multiplier();

  m *= 1.0 + talents.savage_combat -> base_value( E_APPLY_AURA, A_MOD_ATTACK_POWER_PCT );
  
  m *= 1.0 + spec_vitality -> base_value( E_APPLY_AURA, A_MOD_ATTACK_POWER_PCT );

  return m;
}

// rogue_t::composite_player_multiplier =====================================

double rogue_t::composite_player_multiplier( const school_type school ) SC_CONST
{
  double m = player_t::composite_player_multiplier( school );

  if ( buffs_bandits_guile -> check() )
    m *= 1.0 + buffs_bandits_guile -> value();
  
  if ( buffs_master_of_subtlety -> check() )
    m *= 1.0 + buffs_master_of_subtlety -> value();

  if ( buffs_killing_spree -> check() )
    m *= 1.0 + buffs_killing_spree -> value();

  if ( buffs_vendetta -> check() )
    m *= 1.0 + buffs_vendetta -> value();

  if ( sim -> target -> debuffs.bleeding -> check() )
    m *= 1.0 + talents.sanguinary_vein -> base_value() / 100.0;

  return m;
}

// rogue_t::init_actions ===================================================

void rogue_t::init_actions()
{
  if ( main_hand_weapon.type == WEAPON_NONE )
  {
    sim -> errorf( "Player %s has no weapon equipped at the Main-Hand slot.", name() );
    quiet = true;
    return;
  }

  if ( action_list_str.empty() )
  {
    action_list_str += "flask,type=endless_rage";
    action_list_str += "/food,type=blackened_dragonfin";
    action_list_str += "/apply_poison,main_hand=instant,off_hand=deadly";
    action_list_str += "/speed_potion,if=!in_combat|buff.bloodlust.react|target.time_to_die<20";
    action_list_str += "/auto_attack";
    action_list_str += "/snapshot_stats";
    
    if ( talents.overkill -> rank() || spec_master_of_subtlety -> ok() )
      action_list_str += "/stealth";

    action_list_str += "/kick";

    int num_items = ( int ) items.size();
    for ( int i=0; i < num_items; i++ )
    {
      if ( items[ i ].use.active() )
      {
        action_list_str += "/use_item,name=";
        action_list_str += items[ i ].name();
      }
    }

    if ( race == RACE_ORC )
    {
      action_list_str += "/blood_fury";
    }
    else if ( race == RACE_TROLL )
    {
      action_list_str += "/berserking";
    }
    else if ( race == RACE_BLOOD_ELF )
    {
      action_list_str += "/arcane_torrent";
    }

    if ( primary_tree() == TREE_ASSASSINATION )
    {
      action_list_str += "/garrote";
      action_list_str += "/slice_and_dice,if=buff.slice_and_dice.down";
      if ( ! talents.cut_to_the_chase -> rank() )
      {
        action_list_str += "/pool_energy,if=energy<60&buff.slice_and_dice.remains<5";
        action_list_str += "/slice_and_dice,if=combo_points>=3&buff.slice_and_dice.remains<2";
      }
      if ( talents.vendetta -> rank() )
        action_list_str += "/vendetta";
      // XXX tweak Rupture/Envenom priorities to raise Rupture (and Envenom) uptime
      action_list_str += "/rupture,if=!ticking&combo_points>=4&target.time_to_die>15&buff.slice_and_dice.remains>6";
      if ( talents.cold_blood -> rank() ) 
        action_list_str += "/cold_blood,sync=envenom";
      action_list_str += "/envenom,if=combo_points>=4&buff.envenom.down";
      action_list_str += "/envenom,if=combo_points>=4&energy>90";
      action_list_str += "/envenom,if=combo_points>=2&buff.slice_and_dice.remains<2";
      action_list_str += "/backstab,if=combo_points<4&target.health_pct<35";
      action_list_str += "/mutilate,if=combo_points<4&target.health_pct>=35";
      if ( talents.overkill -> rank() ) 
        action_list_str += "/vanish,if=time>30&energy>50";
    }
    else if ( primary_tree() == TREE_COMBAT )
    {
      // XXX TODO
      action_list_str += "/slice_and_dice,if=buff.slice_and_dice.down&time<4";
      action_list_str += "/slice_and_dice,if=buff.slice_and_dice.remains<2&combo_points>=3";
      if ( talents.killing_spree -> rank() )
        action_list_str += "/killing_spree,if=energy<20&buff.slice_and_dice.remains>5";
      action_list_str += "/rupture,if=!ticking&combo_points=5&target.time_to_die>10";
      action_list_str += "/eviscerate,if=combo_points=5&buff.slice_and_dice.remains>7&dot.rupture.remains>6";
      action_list_str += "/eviscerate,if=combo_points>=4&buff.slice_and_dice.remains>4&energy>40&dot.rupture.remains>5";
      action_list_str += "/eviscerate,if=combo_points=5&target.time_to_die<10";
      action_list_str += "/revealing_strike,if=combo_points=4&buff.slice_and_dice.remains>8";
      action_list_str += "/sinister_strike,if=combo_points<5";
      if ( talents.adrenaline_rush -> rank() ) 
        action_list_str += "/adrenaline_rush,if=energy<20";
    }
    else if ( primary_tree() == TREE_SUBTLETY )
    {
      // XXX Tweaks needed. Rupture and Slice and Dice fall off quite reliably.

      if ( talents.shadow_dance -> rank() )
      {
        action_list_str += "/pool_energy,for_next=1";
        action_list_str += "/shadow_dance,if=time>10&energy>75&combo_points<=1&cooldown.shadowstep.remains<=0";
      }
      action_list_str += "/pool_energy,for_next=1";
      action_list_str += "/vanish,if=time>10&energy>60&combo_points<=1&cooldown.shadowstep.remains<=0&!buff.shadow_dance.up";
      
      action_list_str += "/shadowstep,if=buff.stealthed.up|buff.shadow_dance.up";
      if ( talents.premeditation -> rank() )
        action_list_str += "/premeditation";
      action_list_str += "/garrote";
      action_list_str += "/ambush,if=combo_points<=2";

      if ( talents.preparation -> rank() )
        action_list_str += "/preparation,if=cooldown.vanish.remains>60";

      action_list_str += "/slice_and_dice,if=buff.slice_and_dice.remains<2";
      action_list_str += "/rupture,if=combo_points=5&!ticking";

      if ( talents.energetic_recovery -> rank() )
        action_list_str += "/recuperate,if=combo_points=5&dot.rupture.remains>8&buff.slice_and_dice.remains>8";

      action_list_str += "/eviscerate,if=combo_points=5&dot.rupture.remains>1";
      action_list_str += "/eviscerate,if=combo_points>=4&buff.shadow_dance.up";

      // if ( talents.hemorrhage -> rank() )

      action_list_str += "/backstab,if=combo_points<4";
      action_list_str += "/backstab,if=cooldown.honor_among_thieves.remains>1.75";
    }
    else
    {
      action_list_str += "/pool_energy,if=energy<60&buff.slice_and_dice.remains<5";
      action_list_str += "/slice_and_dice,if=combo_points>=3&buff.slice_and_dice.remains<2";
      action_list_str += "/sinister_strike,if=combo_points<5";
    }

    action_list_default = 1;
  }

  player_t::init_actions();
}

// rogue_t::create_action  =================================================

action_t* rogue_t::create_action( const std::string& name,
                                  const std::string& options_str )
{
  if ( name == "auto_attack"         ) return new auto_attack_t        ( this, options_str );
  if ( name == "adrenaline_rush"     ) return new adrenaline_rush_t    ( this, options_str );
  if ( name == "ambush"              ) return new ambush_t             ( this, options_str );
  if ( name == "apply_poison"        ) return new apply_poison_t       ( this, options_str );
  if ( name == "backstab"            ) return new backstab_t           ( this, options_str );
  if ( name == "blade_flurry"        ) return new blade_flurry_t       ( this, options_str );
  if ( name == "cold_blood"          ) return new cold_blood_t         ( this, options_str );
  if ( name == "envenom"             ) return new envenom_t            ( this, options_str );
  if ( name == "eviscerate"          ) return new eviscerate_t         ( this, options_str );
  if ( name == "expose_armor"        ) return new expose_armor_t       ( this, options_str );
  if ( name == "feint"               ) return new feint_t              ( this, options_str );
  if ( name == "garrote"             ) return new garrote_t            ( this, options_str );
  if ( name == "hemorrhage"          ) return new hemorrhage_t         ( this, options_str );
  if ( name == "kick"                ) return new kick_t               ( this, options_str );
  if ( name == "killing_spree"       ) return new killing_spree_t      ( this, options_str );
  if ( name == "mutilate"            ) return new mutilate_t           ( this, options_str );
  if ( name == "pool_energy"         ) return new pool_energy_t        ( this, options_str );
  if ( name == "premeditation"       ) return new premeditation_t      ( this, options_str );
  if ( name == "preparation"         ) return new preparation_t        ( this, options_str );
  if ( name == "recuperate"          ) return new recuperate_t         ( this, options_str );
  if ( name == "revealing_strike"    ) return new revealing_strike_t   ( this, options_str );
  if ( name == "rupture"             ) return new rupture_t            ( this, options_str );
  if ( name == "shadow_dance"        ) return new shadow_dance_t       ( this, options_str );
  if ( name == "shadowstep"          ) return new shadowstep_t         ( this, options_str );
  if ( name == "shiv"                ) return new shiv_t               ( this, options_str );
  if ( name == "sinister_strike"     ) return new sinister_strike_t    ( this, options_str );
  if ( name == "slice_and_dice"      ) return new slice_and_dice_t     ( this, options_str );
  if ( name == "stealth"             ) return new stealth_t            ( this, options_str );
  if ( name == "vanish"              ) return new vanish_t             ( this, options_str );
  if ( name == "vendetta"            ) return new vendetta_t           ( this, options_str );
  if ( name == "tricks_of_the_trade" ) return new tricks_of_the_trade_t( this, options_str );

  return player_t::create_action( name, options_str );
}

// rogue_t::create_expression ==============================================

action_expr_t* rogue_t::create_expression( action_t* a, const std::string& name_str )
{
  if ( name_str == "combo_points" )
  {
    struct combo_points_expr_t : public action_expr_t
    {
      combo_points_expr_t( action_t* a ) : action_expr_t( a, "combo_points", TOK_NUM ) {}
      virtual int evaluate() { rogue_t* p = action -> player -> cast_rogue(); result_num = p -> combo_points -> count; return TOK_NUM; }
    };
    return new combo_points_expr_t( a );
  }

  return player_t::create_expression( a, name_str );
}

// rogue_t::init_glyphs ======================================================

void rogue_t::init_glyphs()
{
  glyphs.adrenaline_rush     = new glyph_t( this, "Glyph of Adrenaline Rush"     );
  glyphs.ambush              = new glyph_t( this, "Glyph of Ambush"              );
  glyphs.backstab            = new glyph_t( this, "Glyph of Backstab"            );
  glyphs.blade_flurry        = new glyph_t( this, "Glyph of Blade Flurry"        );
  glyphs.eviscerate          = new glyph_t( this, "Glyph of Eviscerate"          );
  glyphs.expose_armor        = new glyph_t( this, "Glyph of Expose Armor"        );
  glyphs.feint               = new glyph_t( this, "Glyph of Feint"               );
  glyphs.hemorrhage          = new glyph_t( this, "Glyph of Hemorrhage"          );
  glyphs.kick                = new glyph_t( this, "Glyph of Kick"                );
  glyphs.killing_spree       = new glyph_t( this, "Glyph of Killing Spree"       );
  glyphs.mutilate            = new glyph_t( this, "Glyph of Mutilate"            );
  glyphs.preparation         = new glyph_t( this, "Glyph of Preparation"         );
  glyphs.revealing_strike    = new glyph_t( this, "Glyph of Revealing Strike"    );
  glyphs.rupture             = new glyph_t( this, "Glyph of Rupture"             );
  glyphs.shadow_dance        = new glyph_t( this, "Glyph of Shadow Dance"        );
  glyphs.sinister_strike     = new glyph_t( this, "Glyph of Sinister Strike"     );
  glyphs.slice_and_dice      = new glyph_t( this, "Glyph of Slice and Dice"      );
  glyphs.tricks_of_the_trade = new glyph_t( this, "Glyph of Tricks of the Trade" );
  glyphs.vendetta            = new glyph_t( this, "Glyph of Vendetta"            );
  
  std::vector<std::string> glyph_names;
  int num_glyphs = util_t::string_split( glyph_names, glyphs_str, ",/" );

  for ( int i=0; i < num_glyphs; i++ )
  {
    std::string& n = glyph_names[ i ];

    if      ( n == "adrenaline_rush"     ) glyphs.adrenaline_rush     -> enable();
    else if ( n == "ambush"              ) glyphs.ambush              -> enable();
    else if ( n == "backstab"            ) glyphs.backstab            -> enable();
    else if ( n == "blade_flurry"        ) glyphs.blade_flurry        -> enable();
    else if ( n == "eviscerate"          ) glyphs.eviscerate          -> enable();
    else if ( n == "expose_armor"        ) glyphs.expose_armor        -> enable();
    else if ( n == "feint"               ) glyphs.feint               -> enable();
    else if ( n == "hemorrhage"          ) glyphs.hemorrhage          -> enable();
    else if ( n == "killing_spree"       ) glyphs.killing_spree       -> enable();
    else if ( n == "mutilate"            ) glyphs.mutilate            -> enable();
    else if ( n == "preparation"         ) glyphs.preparation         -> enable();
    else if ( n == "revealing_strike"    ) glyphs.revealing_strike    -> enable();
    else if ( n == "rupture"             ) glyphs.rupture             -> enable();
    else if ( n == "shadow_dance"        ) glyphs.shadow_dance        -> enable();
    else if ( n == "sinister_strike"     ) glyphs.sinister_strike     -> enable();
    else if ( n == "slice_and_dice"      ) glyphs.slice_and_dice      -> enable();
    else if ( n == "tricks_of_the_trade" ) glyphs.tricks_of_the_trade -> enable();
    else if ( n == "vendetta"            ) glyphs.vendetta            -> enable();
    // To prevent warning messages....
    else if ( n == "blurred_speed"       ) ;
    else if ( n == "cloak_of_shadows"    ) ;
    else if ( n == "distract"            ) ;
    else if ( n == "fan_of_knives"       ) ;
    else if ( n == "garrote"             ) ;
    else if ( n == "pick_lock"           ) ;
    else if ( n == "pick_pocket"         ) ;
    else if ( n == "safe_fall"           ) ;
    else if ( n == "sprint"              ) ;
    else if ( n == "vanish"              ) ;
    else if ( ! sim -> parent ) 
    {
      sim -> errorf( "Player %s has unrecognized glyph %s\n", name(), n.c_str() );
    }
  }
}

// rogue_t::init_race ======================================================

void rogue_t::init_race()
{
  race = util_t::parse_race_type( race_str );
  switch ( race )
  {
    case RACE_BLOOD_ELF:
    case RACE_DWARF:
    case RACE_GOBLIN:
    case RACE_GNOME:
    case RACE_HUMAN:
    case RACE_ORC:
    case RACE_NIGHT_ELF:
    case RACE_TROLL:
    case RACE_WORGEN:
    case RACE_UNDEAD:
      break;
    default:
      race = RACE_NIGHT_ELF;
      race_str = util_t::race_type_string( race );
  }

  player_t::init_race();
}

// rogue_t::init_base ========================================================

void rogue_t::init_base()
{
  player_t::init_base();

  base_attack_power = ( level * 2 ) - 20;
  initial_attack_power_per_strength = 1.0;
  initial_attack_power_per_agility  = 2.0;

  resource_base[ RESOURCE_ENERGY ] = 100 + spec_assassins_resolve -> base_value( E_APPLY_AURA, A_MOD_INCREASE_ENERGY );

  health_per_stamina      = 10;
  energy_regen_per_second = 10 + spec_vitality -> base_value( E_APPLY_AURA, A_MOD_POWER_REGEN_PERCENT ) / 10.0;

  base_gcd = 1.0;

  diminished_kfactor    = 0.009880;
  diminished_dodge_capi = 0.006870;
  diminished_parry_capi = 0.006870;
}

// rogue_t::init_talents ======================================================

void rogue_t::init_talents()
{
  // Talents

  // Assasination
  talents.cold_blood                 = new talent_t( this, "cold_blood", "Cold Blood" );
  talents.coup_de_grace              = new talent_t( this, "coup_de_grace", "Coup de Grace" );
  talents.cut_to_the_chase           = new talent_t( this, "cut_to_the_chase", "Cut to the Chase" );
  talents.improved_expose_armor      = new talent_t( this, "improved_expose_armor", "Improved Expose Armor" );
  talents.lethality                  = new talent_t( this, "lethality", "Lethality" );    
  talents.master_poisoner            = new talent_t( this, "master_poisoner", "Master Poisoner" );
  talents.murderous_intent           = new talent_t( this, "murderous_intent", "Murderous Intent" );
  talents.overkill                   = new talent_t( this, "overkill", "Overkill" );
  talents.puncturing_wounds          = new talent_t( this, "puncturing_wounds", "Puncturing Wounds" );
  talents.quickening                 = new talent_t( this, "quickening", "Quickening" );
  talents.ruthlessness               = new talent_t( this, "ruthlessness", "Ruthlessness" );
  talents.seal_fate                  = new talent_t( this, "seal_fate", "Seal Fate" );
  talents.vendetta                   = new talent_t( this, "vendetta", "Vendetta" );
  talents.venomous_wounds            = new talent_t( this, "venomous_wounds", "Venomous Wounds" );
  talents.vile_poisons               = new talent_t( this, "vile_poisons", "Vile Poisons" );

  // Combat
  talents.adrenaline_rush            = new talent_t( this, "adrenaline_rush", "Adrenaline Rush" );
  talents.aggression                 = new talent_t( this, "aggression", "Aggression" );
  talents.bandits_guile              = new talent_t( this, "bandits_guile", "Bandit's Guile" );
  talents.combat_potency             = new talent_t( this, "combat_potency", "Combat Potency" ); 
  talents.improved_sinister_strike   = new talent_t( this, "improved_sinister_strike", "Improved Sinister Strike" );
  talents.improved_slice_and_dice    = new talent_t( this, "improved_slice_and_dice", "Improved Slice and Dice" );
  talents.killing_spree              = new talent_t( this, "killing_spree", "Killing Spree" ); 
  talents.lightning_reflexes         = new talent_t( this, "lightning_reflexes", "Lightning Reflexes" );
  talents.precision                  = new talent_t( this, "precision", "Precision" );
  talents.restless_blades            = new talent_t( this, "restless_blades", "Restless Blades" );
  talents.revealing_strike           = new talent_t( this, "revealing_strike", "Revealing Strike" );
  talents.savage_combat              = new talent_t( this, "savage_combat", "Savage Combat" );
  
  // Subtlety
  talents.elusiveness                = new talent_t( this, "elusiveness", "Elusiveness" );
  talents.energetic_recovery         = new talent_t( this, "energetic_recovery", "Energetic Recovery" );
  talents.find_weakness              = new talent_t( this, "find_weakness", "Find Weakness" );
  talents.hemorrhage                 = new talent_t( this, "hemorrhage", "Hemorrhage" );
  talents.honor_among_thieves        = new talent_t( this, "honor_among_thieves", "Honor Among Thieves" );
  talents.improved_ambush            = new talent_t( this, "improved_ambush", "Improved Ambush" );
  talents.initiative                 = new talent_t( this, "initiative", "Initiative" );
  talents.opportunity                = new talent_t( this, "opportunity", "Opportunity" );
  talents.premeditation              = new talent_t( this, "premeditation", "Premeditation" );
  talents.preparation                = new talent_t( this, "preparation", "Preparation" );
  talents.relentless_strikes         = new talent_t( this, "relentless_strikes", "Relentless Strikes" );
  talents.sanguinary_vein            = new talent_t( this, "sanguinary_vein", "Sanguinary Vein" );
  talents.serrated_blades            = new talent_t( this, "serrated_blades", "Serrated Blades" );
  talents.shadow_dance               = new talent_t( this, "shadow_dance", "Shadow Dance" );
  talents.slaughter_from_the_shadows = new talent_t( this, "slaughter_from_the_shadows", "Slaughter from the Shadows" );
  talents.waylay                     = new talent_t( this, "waylay", "Waylay" );

  player_t::init_talents();
}

// rogue_t::init_spells ======================================================

void rogue_t::init_spells()
{
  player_t::init_spells();

  // Spec passives
  spec_improved_poisons   = new passive_spell_t( this, "improved_poisons",   14117 );
  spec_assassins_resolve  = new passive_spell_t( this, "assassins_resolve",  84601 );
  spec_vitality           = new passive_spell_t( this, "vitality",           61329 );
  spec_ambidexterity      = new passive_spell_t( this, "ambidexterity",      13852 );
  spec_master_of_subtlety = new passive_spell_t( this, "master_of_subtlety", 31223 );
  spec_sinister_calling   = new passive_spell_t( this, "sinister_calling",   31220 );

  // Spec Actives
  spec_mutilate           = new active_spell_t( this, "mutilate",            1329 );
  spec_blade_flurry       = new active_spell_t( this, "blade_flurry",        13877 );
  spec_shadowstep         = new active_spell_t( this, "shadowstep",          36554 );

  // Masteries
  mastery_potent_poisons  = new mastery_t( this, "potent_poisons",     76803, TREE_ASSASSINATION );
  mastery_main_gauche     = new mastery_t( this, "main_gauche",        76806, TREE_COMBAT );
  mastery_executioner     = new mastery_t( this, "executioner",        76808, TREE_SUBTLETY );
}

// rogue_t::init_gains =======================================================

void rogue_t::init_gains()
{
  player_t::init_gains();

  gains_adrenaline_rush    = get_gain( "adrenaline_rush"    );
  gains_backstab_glyph     = get_gain( "backstab_glyph"     );
  gains_cold_blood         = get_gain( "cold_blood"         );
  gains_combat_potency     = get_gain( "combat_potency"     );
  gains_energy_refund      = get_gain( "energy_refund"      );
  gains_murderous_intent   = get_gain( "murderous_intent"   );
  gains_overkill           = get_gain( "overkill"           );
  gains_recuperate         = get_gain( "recuperate"         );
  gains_relentless_strikes = get_gain( "relentless_strikes" );
  gains_venomous_vim       = get_gain( "venomous_vim"       );
  gains_tier10_2pc         = get_gain( "tier10_2pc"         );
}

// rogue_t::init_procs =======================================================

void rogue_t::init_procs()
{
  player_t::init_procs();

  procs_deadly_poison         = get_proc( "deadly_poisons"        );
  procs_honor_among_thieves   = get_proc( "honor_among_thieves"   );
  procs_main_gauche           = get_proc( "main_gauche"           );
  procs_ruthlessness          = get_proc( "ruthlessness"          );
  procs_seal_fate             = get_proc( "seal_fate"             );
  procs_serrated_blades       = get_proc( "serrated_blades"       );
  procs_sinister_strike_glyph = get_proc( "sinister_strike_glyph" );
  procs_venomous_wounds       = get_proc( "venomous_wounds"       );
  procs_tier10_4pc            = get_proc( "tier10_4pc"            );
}

// rogue_t::init_uptimes =====================================================

void rogue_t::init_uptimes()
{
  player_t::init_uptimes();

  uptimes_bandits_guile[ 0 ] = get_uptime( "shallow_insight" );
  uptimes_bandits_guile[ 1 ] = get_uptime( "moderate_insight" );
  uptimes_bandits_guile[ 2 ] = get_uptime( "deep_insight" );

  uptimes_energy_cap = get_uptime( "energy_cap" );
  uptimes_poisoned   = get_uptime( "poisoned"   );
  uptimes_rupture    = get_uptime( "rupture" );
}

// rogue_t::init_rng =========================================================

void rogue_t::init_rng()
{
  player_t::init_rng();

  rng_anesthetic_poison     = get_rng( "anesthetic_poison"     );
  rng_bandits_guile         = get_rng( "bandits_guile"         );
  rng_combat_potency        = get_rng( "combat_potency"        );
  rng_cut_to_the_chase      = get_rng( "cut_to_the_chase"      );
  rng_deadly_poison         = get_rng( "deadly_poison"         );
  rng_honor_among_thieves   = get_rng( "honor_among_thieves"   );
  rng_improved_expose_armor = get_rng( "improved_expose_armor" );
  rng_initiative            = get_rng( "initiative"            );
  rng_instant_poison        = get_rng( "instant_poison"        );
  rng_main_gauche           = get_rng( "main_gauche"           );
  rng_relentless_strikes    = get_rng( "relentless_strikes"    );
  rng_ruthlessness          = get_rng( "ruthlessness"          );
  rng_seal_fate             = get_rng( "seal_fate"             );
  rng_serrated_blades       = get_rng( "serrated_blades"       );
  rng_sinister_strike_glyph = get_rng( "sinister_strike_glyph" );
  rng_venomous_wounds       = get_rng( "venomous_wounds"       );
  rng_wound_poison          = get_rng( "wound_poison"          );
  rng_tier10_4pc            = get_rng( "tier10_4pc"            );

  // Overlapping procs require the use of a "distributed" RNG-stream when normalized_rng=1
  // also useful for frequent checks with low probability of proc and timed effect

  rng_critical_strike_interval = get_rng( "critical_strike_interval", RNG_DISTRIBUTED );
  rng_critical_strike_interval -> average_range = false;
}

// rogue_t::init_scaling ====================================================

void rogue_t::init_scaling()
{
  player_t::init_scaling();

  scales_with[ STAT_WEAPON_OFFHAND_DPS    ] = 1;
  scales_with[ STAT_WEAPON_OFFHAND_SPEED  ] = 1;
}

// rogue_t::init_buffs ======================================================

void rogue_t::init_buffs()
{
  player_t::init_buffs();

  // buff_t( sim, player, name, max_stack, duration, cooldown, proc_chance, quiet )
  buffs_bandits_guile      = new buff_t( this, "bandits_guile", 12, 15.0, 0.0, 1.0, true );
  buffs_deadly_proc        = new buff_t( this, "deadly_proc",   1  );
  buffs_overkill           = new buff_t( this, "overkill",      1, 20.0 );
  buffs_poison_doses       = new buff_t( this, "poison_doses",  5  );
  buffs_recuperate         = new buff_t( this, "recuperate",    1  );
  buffs_shiv               = new buff_t( this, "shiv",          1  );
  buffs_stealthed          = new buff_t( this, "stealthed",     1  );
  buffs_tier11_4pc         = new buff_t( this, "tier11_4pc",    1, 15.0, 0.0, set_bonus.tier11_4pc_melee() * 0.01 );
  buffs_vanish             = new buff_t( this, "vanish",        1, 3.0 );

  buffs_blade_flurry       = new new_buff_t( this, "blade_flurry",   spec_blade_flurry -> spell_id() );
  buffs_cold_blood         = new new_buff_t( this, "cold_blood",     talents.cold_blood -> spell_id() );
  buffs_shadow_dance       = new new_buff_t( this, "shadow_dance",   talents.shadow_dance -> spell_id() );

  buffs_adrenaline_rush    = new adrenaline_rush_buff_t    ( this, talents.adrenaline_rush -> spell_id() );
  buffs_envenom            = new envenom_buff_t            ( this );
  buffs_find_weakness      = new find_weakness_buff_t      ( this, talents.find_weakness -> spell_id() );
  buffs_killing_spree      = new killing_spree_buff_t      ( this, talents.killing_spree -> spell_id() );
  buffs_master_of_subtlety = new master_of_subtlety_buff_t ( this, spec_master_of_subtlety -> spell_id()  );
  buffs_revealing_strike   = new revealing_strike_buff_t   ( this, talents.revealing_strike -> spell_id() );
  buffs_shadowstep         = new shadowstep_buff_t         ( this, spec_shadowstep -> effect_trigger_spell( 1 ) );
  buffs_slice_and_dice     = new slice_and_dice_buff_t     ( this );
  buffs_vendetta           = new vendetta_buff_t           ( this, talents.vendetta -> spell_id() );
}

// rogue_t::init_values ====================================================

void rogue_t::init_values()
{
  player_t::init_values();
}

// trigger_honor_among_thieves =============================================

struct honor_among_thieves_callback_t : public action_callback_t
{
  honor_among_thieves_callback_t( rogue_t* r ) : action_callback_t( r -> sim, r ) {}

  virtual void trigger( action_t* a )
  {
    rogue_t* rogue = listener -> cast_rogue();

    if ( a )
    {
      if ( ! a -> special || a -> aoe || a -> pseudo_pet || a -> proc )
        return;

      a -> player -> procs.hat_donor -> occur();
    }

    if ( rogue -> cooldowns_honor_among_thieves -> remains() > 0 )
      return;

    if ( ! rogue -> rng_honor_among_thieves -> roll( rogue -> talents.honor_among_thieves -> proc_chance() ) ) 
      return;

    rogue -> combo_points -> add( 1, rogue -> talents.honor_among_thieves );

    rogue -> procs_honor_among_thieves -> occur();

    rogue -> cooldowns_honor_among_thieves -> start( 5.0 - rogue -> talents.honor_among_thieves -> rank() );
  }
};

// rogue_t::register_callbacks ===============================================

void rogue_t::register_callbacks()
{
  player_t::register_callbacks();

  if ( talents.honor_among_thieves -> rank() )
  {
    action_callback_t* cb = new honor_among_thieves_callback_t( this );

    register_attack_direct_result_callback( RESULT_CRIT_MASK, cb );

    if ( sim -> player_list -> next )
    {
      for ( player_t* p = sim -> player_list; p; p = p -> next )
      {
        if ( p == this     ) continue;
        if ( p -> is_pet() ) continue;

        cb = new honor_among_thieves_callback_t( this );

        p -> register_attack_direct_result_callback( RESULT_CRIT_MASK, cb );
      }
    }
    else // Virtual Party
    {
      std::vector<std::string> intervals;
      int num_intervals = util_t::string_split( intervals, critical_strike_intervals_str, ",;|/" );

      if ( num_intervals == 0 )
      {
        intervals.push_back( "1.0" );
        num_intervals = 1;
      }

      while ( num_intervals < 4 )
      {
        intervals.push_back( intervals[ num_intervals-1 ] );
        num_intervals++;
      }

      for ( int i=0; i < num_intervals; i++ )
      {
        critical_strike_intervals.push_back( atof( intervals[ i ].c_str() ) );
        critical_strike_callbacks.push_back( new honor_among_thieves_callback_t( this ) );
      }
    }
  }
}

// rogue_t::combat_begin ===================================================

void rogue_t::combat_begin()
{
  player_t::combat_begin();

  if ( talents.honor_among_thieves -> rank() )
  {
    if ( ! sim -> player_list -> next ) // Virtual Party
    {
      int num_intervals = ( int ) critical_strike_intervals.size();
      for ( int i=0; i < num_intervals; i++ )
      {
        struct critical_strike_t : public event_t
        {
          action_callback_t* callback;
          double interval;

          critical_strike_t( sim_t* sim, rogue_t* p, action_callback_t* cb, double i ) :
            event_t( sim, p ), callback(cb), interval(i)
          {
            name = "Critical Strike";
            double time = p -> rng_critical_strike_interval -> range( interval*0.5, interval*1.5 );
            sim -> add_event( this, time );
          }
          virtual void execute()
          {
            rogue_t* p = player -> cast_rogue();
            callback -> trigger( NULL );
            new ( sim ) critical_strike_t( sim, p, callback, interval );
          }
        };

        new ( sim ) critical_strike_t( sim, this, critical_strike_callbacks[ i ], critical_strike_intervals[ i ] );
      }
    }
  }
}

// rogue_t::reset ===========================================================

void rogue_t::reset()
{
  player_t::reset();

  _expirations.reset();

  // Reset the callbacks for the Virtual Party
  int num_intervals = ( int ) critical_strike_callbacks.size();
  for ( int i=0; i < num_intervals; i++ )
  {
    critical_strike_callbacks[ i ] -> reset();
  }

  tricks_of_the_trade_target = 0;
}

// rogue_t::interrupt ======================================================

void rogue_t::interrupt()
{
  player_t::interrupt();

  if ( main_hand_attack ) main_hand_attack -> cancel();
  if (  off_hand_attack )  off_hand_attack -> cancel();
}

// rogue_t::clear_debuffs ==================================================

void rogue_t::clear_debuffs()
{
  player_t::clear_debuffs();

  buffs_poison_doses -> expire();
  combo_points -> clear();
}

// rogue_t::regen ==========================================================

void rogue_t::regen( double periodicity )
{  
  // XXX review how this all stacks (additive/multiplicative)
  
  // haste (from rating) boosts energy regen by * ( 1 + haste% )
  periodicity *= 1.0 + haste_rating / rating.attack_haste;
  
  if ( buffs_blade_flurry -> up() )
    periodicity *= glyphs.blade_flurry -> ok() ? 0.90 : 0.80;

  player_t::regen( periodicity );

  if ( buffs_adrenaline_rush -> up() )
  {
    if ( sim -> infinite_resource[ RESOURCE_ENERGY ] == 0 )
    {
      double energy_regen = periodicity * energy_regen_per_second;

      resource_gain( RESOURCE_ENERGY, energy_regen, gains_adrenaline_rush );
    }
  }

  if ( buffs_overkill -> up() )
  {
    if ( sim -> infinite_resource[ RESOURCE_ENERGY ] == 0 )
    {
      double energy_regen = periodicity * energy_regen_per_second * 0.30;

      resource_gain( RESOURCE_ENERGY, energy_regen, gains_overkill );
    }
  }

  uptimes_energy_cap -> update( resource_current[ RESOURCE_ENERGY ] ==
                                resource_max    [ RESOURCE_ENERGY ] );

  uptimes_rupture -> update( dots_rupture -> ticking );

  for ( int i = 0; i < 3; i++ )
    uptimes_bandits_guile[ i ] -> update( ( buffs_bandits_guile -> current_stack / 4 - 1 ) == i );
}

// rogue_t::available ======================================================

double rogue_t::available() SC_CONST
{
  double energy = resource_current[ RESOURCE_ENERGY ];

  if ( energy > 25 )
    return 0.1;

  return std::max( ( 25 - energy ) / energy_regen_per_second, 0.1 );
}

// rogue_t::get_talent_trees ==============================================

std::vector<talent_translation_t>& rogue_t::get_talent_list()
{
  talent_list.clear();
  return talent_list;
}

// rogue_t::get_options ================================================

std::vector<option_t>& rogue_t::get_options()
{
  if ( options.empty() )
  {
    player_t::get_options();

    option_t rogue_options[] =
    {
      // @option_doc loc=player/rogue/misc title="Misc"
      { "critical_strike_intervals",  OPT_STRING, &( critical_strike_intervals_str  ) },
      { "tricks_of_the_trade_target", OPT_STRING, &( tricks_of_the_trade_target_str ) },
      { NULL, OPT_UNKNOWN, NULL }
    };

    option_t::copy( options, rogue_options );
  }

  return options;
}

// rogue_t::create_profile =================================================

bool rogue_t::create_profile( std::string& profile_str, int save_type )
{
  player_t::create_profile( profile_str, save_type );

  if ( save_type == SAVE_ALL || save_type == SAVE_ACTIONS )
  {
    if ( talents.honor_among_thieves -> rank() )
    {
      profile_str += "# When using this profile with a real raid AND party setups, this parameter will be ignored.\n";
      profile_str += "# These values represent the avg donor intervals (unlimited by cooldown) of the Rogues's party members.\n";
      profile_str += "# This does not affect HAT procs generated by the Rogue himself.\n";
      profile_str += "critical_strike_intervals=" + critical_strike_intervals_str + "\n";
    }
  }

  return true;
}

// rogue_t::decode_set =====================================================

int rogue_t::decode_set( item_t& item )
{
  if ( item.slot != SLOT_HEAD      &&
       item.slot != SLOT_SHOULDERS &&
       item.slot != SLOT_CHEST     &&
       item.slot != SLOT_HANDS     &&
       item.slot != SLOT_LEGS      )
  {
    return SET_NONE;
  }

  const char* s = item.name();

  if ( strstr( s, "shadowblade"  ) ) return SET_T10_MELEE;
  if ( strstr( s, "wind_dancers" ) ) return SET_T11_MELEE;

  return SET_NONE;
}

// ==========================================================================
// PLAYER_T EXTENSIONS
// ==========================================================================

// player_t::create_rogue  ==================================================

player_t* player_t::create_rogue( sim_t* sim, const std::string& name, race_type r )
{
  return new rogue_t( sim, name, r );
}

// player_t::rogue_init =====================================================

void player_t::rogue_init( sim_t* sim )
{
  sim -> auras.honor_among_thieves = new aura_t( sim, "honor_among_thieves" );

  for ( player_t* p = sim -> player_list; p; p = p -> next )
  {
    p -> buffs.tricks_of_the_trade = new buff_t( p, "tricks_of_the_trade", 1, 6.0 );
  }

  for ( target_t* t = sim -> target_list; t; t = t -> next )
  {
    t -> debuffs.expose_armor    = new debuff_t( t, "expose_armor",     1 );
    t -> debuffs.hemorrhage      = new debuff_t( t, "hemorrhage",       1, 60.0 );
    t -> debuffs.master_poisoner = new debuff_t( t, "master_poisoner", -1 );
    t -> debuffs.poisoned        = new debuff_t( t, "poisoned",        -1 );
    t -> debuffs.savage_combat   = new debuff_t( t, "savage_combat",   -1 );
  }
}

// player_t::rogue_combat_begin =============================================

void player_t::rogue_combat_begin( sim_t* sim )
{
  if ( sim -> overrides.honor_among_thieves ) 
    sim -> auras.honor_among_thieves -> override();

  for ( target_t* t = sim -> target_list; t; t = t -> next )
  {
    if ( sim -> overrides.expose_armor    ) t -> debuffs.expose_armor    -> override( 1, 0.12 );
    if ( sim -> overrides.hemorrhage      ) t -> debuffs.hemorrhage      -> override();
    if ( sim -> overrides.master_poisoner ) t -> debuffs.master_poisoner -> override();
    if ( sim -> overrides.poisoned        ) t -> debuffs.poisoned        -> override();
    if ( sim -> overrides.savage_combat   ) t -> debuffs.savage_combat   -> override();
  }
}
