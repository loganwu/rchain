package coop.rchain.p2p.effects

import scala.concurrent.duration.Duration

import cats._, cats.data._

import coop.rchain.catscontrib.{MonadTrans, _}
import coop.rchain.catscontrib.Catscontrib._
import coop.rchain.comm.ProtocolNode

trait Ping[F[_]] {
  def ping(localNode: ProtocolNode, remoteNode: ProtocolNode): F[Option[Duration]]
}

object Ping extends PingInstances {
  def apply[F[_]](implicit P: Ping[F]): Ping[F] = P

  def forTrans[F[_]: Monad, T[_[_], _]: MonadTrans](implicit P: Ping[F]): Ping[T[F, ?]] =
    new Ping[T[F, ?]] {
      def ping(localNode: ProtocolNode, remoteNode: ProtocolNode): T[F, Option[Duration]] =
        P.ping(localNode, remoteNode).liftM[T]
    }
}

sealed abstract class PingInstances {
  implicit def eitherTPing[E, F[_]: Monad: Ping[?[_]]]: Ping[EitherT[F, E, ?]] =
    Ping.forTrans[F, EitherT[?[_], E, ?]]
}
